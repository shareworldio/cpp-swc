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
/** @file Ethash.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 *
 * A proof of work algorithm.
 */

#pragma once

#include <sys/types.h>

#include <libethcore/SealEngine.h>
#include <libethereum/GenericFarm.h>
#include <libp2p/Capability.h>
#include <libp2p/Common.h>
#include <libethereum/ChainParams.h>
#include <libethereum/BlockChain.h>
#include <libethcore/KeyManager.h>

#include "QposHost.h"
#include "QposPeer.h"

#include <atomic>
#include <string>
#include <vector>
#include <set>
#include <map>

using namespace std;
using namespace dev;
using namespace eth;

namespace dev
{

using namespace p2p;

namespace eth
{

class QposHost;
class QposPeer;

enum QposAccountType {
	EN_ACCOUNT_TYPE_NORMAL = 0,
	EN_ACCOUNT_TYPE_MINER = 1
};

class QposNode
{
public:
	QposNode(NodeID _id, std::map<string, string> _property):m_id(_id), m_property(_property){};
	bool operator < (const QposNode& _d) const{return this->m_id < _d.m_id;};
	
	NodeID m_id;
	map<string, string> m_property;
};

class QposSealEngine: public eth::SealEngineBase//, Worker
{
public:
	QposSealEngine();
	std::string name() const override { return "QPOS"; }
	static void init();
	
	virtual void initEnv(class Client *_c, p2p::Host *_host, BlockChain* _bc, bool _importAnyNode);

	void startGeneration();
	//void cancelGeneration() override { stopWorking(); }
	
	virtual bool shouldSeal(Interface*){return true;};
	void populateFromParent(BlockHeader& _bi, BlockHeader const& _parent) const;
	virtual bool interpret(QposPeer*, unsigned _id, RLP const& _r);

	EVMSchedule const& evmSchedule(u256 const& _blockNumber) const override;

	//bool noteNewBlocks() const override { return true; }
	virtual void generateSeal(bytes const& _block) = 0;
	void addPeers(std::set<QposNode>& _nodes);
protected:
	virtual void tick();
	bool getMinerList();
	bool getMinerList(set<NodeID> &_miner_list, BlockNumber _blk_no = LatestBlock) const;

	bool getNodes(set<QposNode> &_miner_list);
		
	Signature sign(h256 const& _hash){return dev::sign(m_pair.secret(), _hash);};
	bool verify(Signature const& _s, h256 const& _hash){return dev::verify(m_pair.pub(), _s, _hash);};

	void send(const NodeID &_id, RLPStream& _msg);
	void multicast(const set<NodeID> &_id, RLPStream& _msg);
	void broadcast(RLPStream& _msg);
	void multicast(const h512s &_miner_list, RLPStream& _msg);

	bool verifyBlock(const bytes& _block, ImportRequirements::value _ir = ImportRequirements::OutOfOrderChecks) const;

	NodeID id() const{ return m_pair.pub();};

	std::vector<p2p::NodeSpec> exNodes() { return m_exNodes;}
private:
	virtual void workLoop();	
private:
	Client* m_client;
	p2p::Host* m_p2pHost;
	BlockChain* m_bc;

	std::shared_ptr<QposHost> m_LeaderHost;
	
	KeyPair m_pair = KeyPair::create();
	std::vector<p2p::NodeSpec> m_exNodes;
	
	bool exnodesMe = false;
	bool exnodesAnyone = false;
	
	RecursiveMutex x_nodes;
	std::set<QposNode> m_nodes;
	std::string m_nodes_str;
	bool m_nodes_changed = true;
};

}
}
