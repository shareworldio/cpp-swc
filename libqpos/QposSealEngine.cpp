/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Ethash.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include <libethereum/Interface.h>
#include <libethcore/ChainOperationParams.h>
#include <libethcore/CommonJS.h>
#include <libethcore/ABI.h>
#include <libp2p/Host.h>

#include <libethereum/Client.h>
#include <libethereum/EthereumHost.h>

#include <tr1/memory>
#include <boost/algorithm/string.hpp>
#include <libdevcore/JsonUtils.h>

#include "QposSealEngine.h"
#include "Common.h"
#include "Qpos.h"
#include <libsolidity/Solidity.h>

using namespace std;
using namespace dev;
using namespace eth;
using namespace p2p;

namespace js = json_spirit;

#define CONTANT_NODE "0x0000000000000000000000000000000000000009"

QposSealEngine::QposSealEngine()
{
}

void QposSealEngine::initEnv(class Client *_c, p2p::Host *_host, BlockChain* _bc, bool _importAnyNode)
{
	(void)_importAnyNode;
	
	m_client = _c;
	m_p2pHost = _host;
	m_bc = _bc;
	
	m_LeaderHost.reset(new QposHost(this));
	
	m_pair = _host->keyPair();
	
	std::shared_ptr<QposHost> ptr(m_LeaderHost);// = std::make_shared<LeaderHostCapability>();
	m_p2pHost->registerCapability(ptr);

	for(auto it : m_client->chainParams().exnodes){
		//m_p2pHost->addPeer(it, PeerType::Required);
		m_exNodes.push_back(it);
	}

	const Address addr = jsToAddress(nodeAddress());
	m_client->onImprted(addr, [&]()
	{
			this->getMinerList();
	});
	
	exnodesMe = m_client->chainParams().exnodesMe;
	exnodesAnyone = m_client->chainParams().exnodesAnyone;

	cdebug << "m_pair.pub()=" << m_pair.pub() << ",exnodesMe=" << exnodesMe << ",exnodesAnyone=" << exnodesAnyone;
}

void QposSealEngine::init()
{
	static SealEngineFactory __eth_registerSealEngineFactoryQpos = SealEngineRegistrar::registerSealEngine<Qpos>("QPOS");
}

void QposSealEngine::populateFromParent(BlockHeader& _bi, BlockHeader const& _parent) const
{
	SealEngineFace::populateFromParent(_bi, _parent);
	
	cdebug << "_parent.gasLimit()=" << _parent.gasLimit();
	_bi.setGasLimit(_parent.gasLimit());
	_bi.setDifficulty(u256(1));
}

void QposSealEngine::send(const NodeID &_id, RLPStream& _msg)
{
	set<NodeID> idset = {_id};

	multicast(idset, _msg);
}

void QposSealEngine::multicast(const set<NodeID> &_id, RLPStream& _msg)
{
	if(_id.empty())
		return;

	m_LeaderHost->foreachPeer([&](std::shared_ptr<QposPeer> _p)
	{
		if(_id.count(_p->id()) && _p->id() != id()){
			RLPStream s;
			_p->prep(s, QposMsgQpos, 1);
			s.appendList(_msg);
			_p->sealAndSend(s);
		}
		return true;
	});

}

void QposSealEngine::broadcast(RLPStream& _msg)
{		
	m_LeaderHost->foreachPeer([&](std::shared_ptr<QposPeer> _p)
	{
		RLPStream s;
		_p->prep(s, QposMsgQpos, 1);
		s.appendList(_msg);
		_p->sealAndSend(s);
		return true;
	});

}

void QposSealEngine::multicast(const h512s &miner_list, RLPStream& _msg)
{
	set<NodeID> idset;
	idset.insert(miner_list.begin(), miner_list.end());

	multicast(idset, _msg);
}

bool QposSealEngine::verifyBlock(const bytes& _block, ImportRequirements::value _ir) const
{
	try{
		m_bc->verifyBlock(&_block, nullptr, _ir);
	}catch(...){
		return false;
	}

	return true;
}

void QposSealEngine::tick() 
{
}

void QposSealEngine::workLoop()
{/*
	while (isWorking()){
		this->tick();
	
		this_thread::sleep_for(std::chrono::milliseconds(100));
	}*/
}

std::map<string,string> getValueMap(js::mValue &v)
{
	std::map<string,string> m;
	
	if (v.type() != json_spirit::obj_type){
	     return m;
	}
	
	try{
		for (auto const& it: v.get_obj()){
			if(it.second.type() != json_spirit::str_type)
				continue;
			
			m[it.first] = it.second.get_str();	
		}
	}catch(...){
		m.clear();
	}

	return m;
}

std::set<QposNode> strToNode(string _str)
{
	std::set<QposNode> nodes;
	js::mValue val;
	json_spirit::read_string_or_throw(_str, val);
	js::mArray array = val.get_array();

	cdebug << "_str=" << _str;
	for (size_t i = 0; i < array.size(); ++i)
	{
		try{
			js::mValue v = array[i];
			js::mObject o = v.get_obj();
			auto it = o.find("id");
			auto& codeObj = it->second;

            if (codeObj.type() != json_spirit::str_type)
            {
            	continue;
            }

			auto& id = codeObj.get_str();
			std::map<string,string> value;
			it = o.find("property");
			if(it != o.end())
				value = getValueMap(it->second);

			cdebug << "i=" << i << ",id=" << id;
			nodes.insert(QposNode(NodeID(id), value));
		}catch(...){
			cdebug << "parse json err i=" << i;
		}
	}

	return nodes;
}

bool QposSealEngine::getMinerList() 
{
	DEV_RECURSIVE_GUARDED(x_nodes)
	{
		string out = m_client->getNodes("");
		if(m_nodes_str == out)
			return false;
		
		m_nodes_str = out;
		m_nodes_changed = true;
		m_nodes = (strToNode(m_nodes_str));
		cdebug << "m_nodes_str=" << m_nodes_str;
	}

	return true;
}

bool QposSealEngine::getMinerList(set<NodeID> &_miner_list, int _blk_no) const 
{
	(void)_miner_list;
	(void)_blk_no;

	return true;
}

bool QposSealEngine::getNodes(set<QposNode> &_miner_list) 
{
	DEV_RECURSIVE_GUARDED(x_nodes)
	{
		if(!m_nodes_changed)
			return false;

		cdebug << "m_nodes_str=" << m_nodes_str;
		_miner_list = m_nodes;
	}

	return true;
}

bool QposSealEngine::interpret(QposPeer* _peer, unsigned _id, RLP const& _r) 
{
	(void)_peer;
	(void)_id;
	(void)_r;

	return true;
}

EVMSchedule const& QposSealEngine::evmSchedule(u256 const& _blockNumber) const
{
	(void)_blockNumber;
	return DefaultSchedule;
}

void QposSealEngine::addPeers(std::set<QposNode>& _nodes)
{
	for(auto it : _nodes){
		if(it.m_id == id())
			continue;
		
		map<string, string> &property = it.m_property;

		try{
			if(property.count("ip") && property["ip"].length() > 0 && property.count("port") && property["port"].length() > 0){
				string spec = string("enode://") + it.m_id.hex() + string("@") + property["ip"] + string(":") + property["port"];
				cdebug << "spec=" << spec;
				m_p2pHost->addPeer(p2p::NodeSpec(spec), p2p::PeerType::RequiredNotExist);
			}
		}catch(...){
		}
	}
}

void QposSealEngine::startGeneration()
{
	this->getMinerList();
}


