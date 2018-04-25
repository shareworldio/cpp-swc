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
/** @file BlockChainSync.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "BlockChainRequest.h"

#include <chrono>
#include <libdevcore/Common.h>
#include <libdevcore/TrieHash.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>
#include <libethcore/Exceptions.h>
#include <libdevcore/CommonJS.h>

#include "BlockChain.h"
#include "BlockQueue.h"
#include "EthereumPeer.h"
#include "EthereumHost.h"

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace p2p;

#define MAX_REQUEST_COUNT 8
const unsigned REQUEST_BODY_TIMEOUT = 10000;

Blocker::Blocker(bytesConstRef _block, unsigned _type, NodeID _id)
{
	id = _id;
	time = utcTime();
	switch(_type){
		case blockerHead:
			{
				BlockHeader header(_block, HeaderData);
				hash = header.hash();
				head = _block.toBytes();
				parent = header.parentHash();
				cdebug << "Blocker_head:header.number()=" << header.number() << ",header.hash()=" << header.hash() << ",_id=" << _id << ",time=" << time;
			}
			break;
		case blockerBlock:
			{
				BlockHeader header(_block);
				RLP block(_block);

				head = block[0].data().toBytes();
				
				RLPStream blockStream(block.itemCount()-1);
				for(size_t i = 1; i < block.itemCount(); i++)
					blockStream.appendRaw(block[i].data());
				blockStream.swapOut(body);
				
				hash = header.hash();
				parent = header.parentHash();

				RLP rlpbody(body);
				auto txList = rlpbody[0];
				h256 transactionRoot = trieRootOver(txList.itemCount(), [&](unsigned i){ return rlp(i); }, [&](unsigned i){ return txList[i].data().toBytes(); });
				h256 uncles = sha3(rlpbody[1].data());

				if(header.sha3Uncles() != uncles || transactionRoot != header.transactionsRoot()){
					cdebug << "BOOST_THROW_EXCEPTION: uncles=" << uncles << "header.sha3Uncles()=" << header.sha3Uncles() << ",transactionRoot=" << transactionRoot << ",header.transactionsRoot()=" << header.transactionsRoot() << ",_id=" << _id << ",time=" << time;
					BOOST_THROW_EXCEPTION(BadRLP());
				}

				cdebug << "Blocker_block:header.number()=" << header.number() << ",header.hash()=" << header.hash() << ",_id=" << _id << ",time=" << time;
			}
			break;
		default:
			break;
	}	
}

void Request::clear()
{
	m_blocks.clear();
	m_HeaderToNumber.clear();
}

void Request::insertBlocker(unsigned blockNumber, Blocker&& _block, const BlockHeader& _header)
{
	assert(blockNumber >= 1);
	if(1 == blockNumber){
		m_blocks[0] = m_genesis;
		assert(_block.parent == m_genesis.hash);
	}
	
	if (_header.transactionsRoot() == EmptyTrie && _header.sha3Uncles() == EmptyListSHA3)
	{
		//empty body, just mark as downloaded
		RLPStream r(2);
		r.appendRaw(RLPEmptyList);
		r.appendRaw(RLPEmptyList);
		r.swapOut(_block.body);
		m_blocks[blockNumber] = move(_block);
		cdebug << "insert empty body blockNumber=" << blockNumber;
		return;
	}

	//不同高度的块可能有相同的transactionsRoot
	m_blocks[blockNumber] = move(_block);
	Header headerId { _header.transactionsRoot(), _header.sha3Uncles() };
	cdebug << "insert_head:blockNumber=" << blockNumber << ",_header.hash()=" << toJS(_header.hash()) << ",_header.parentHash()=" << _header.parentHash() << ",_header.transactionsRoot()=" << _header.transactionsRoot() << ",_header.sha3Uncles()=" << _header.sha3Uncles();
	
	if(m_HeaderToNumber.count(headerId)){
		cdebug << "same_transactionsRoot:blockNumber=" << blockNumber << ",_header.hash()=" << toJS(_header.hash()) << ",_header.parentHash()=" << _header.parentHash() << ",_header.transactionsRoot()=" << _header.transactionsRoot() << ",_header.sha3Uncles()=" << _header.sha3Uncles() << ",m_HeaderToNumber[headerId]=" << m_HeaderToNumber[headerId];
		//assert(0 == m_HeaderToNumber.count(headerId));
	}
	m_HeaderToNumber[headerId] = blockNumber;
}

void Request::insertBlocker(Blocker&& _blocker)
{
	BlockHeader header(_blocker.head, HeaderData);
	unsigned blockNumber = header.number();
	assert(blockNumber > 0);

	if(blockNumber > upNumber()){
		cdebug << "set_new_id: blockNumber=" << blockNumber << ",_blocker.hash=" << _blocker.hash << ",_blocker.id=" << _blocker.id << ",id()=" << id() << ",upNumber()=" << upNumber();

		auto parent = m_blocks.find(blockNumber-1);
		if(parent != m_blocks.end() && header.parentHash() != parent->second.hash){
			cdebug << "start_clear_block: blockNumber=" << blockNumber << "header.hash()=" << header.hash() << ",parent->second.parent=" << parent->second.parent << ",parent->second.hash=" << parent->second.hash;
			clear();
		}
		
		insertBlocker(blockNumber, move(_blocker), header);
		return;
	}

	if(_blocker.id != id()){
		cdebug << "recv_no_expect_block: blockNumber=" << blockNumber << ",_blocker.hash=" << _blocker.hash << ",_blocker.id=" << _blocker.id << ",id()=" << id() << ",upNumber()=" << upNumber();
		return;
	}

	uint64_t uptime = upTime();
	
	auto child = m_blocks.find(blockNumber+1);
	if(child != m_blocks.end() && child->second.parent != header.hash()){
		cdebug << "remove_child_unmatch: blockNumber=" << blockNumber << "header.hash()=" << header.hash() << ",child->second.parent=" << child->second.parent;
		if(child->second.time > uptime){
			cdebug << "clear_all:child->second.time=" << child->second.time << ",uptime=" << uptime << ",underHash()=" << underHash();
			clear();
			return;
		}else{
			removeUpWith(blockNumber);
		}
	}

	auto parent = m_blocks.find(blockNumber-1);
	if(parent != m_blocks.end() && header.parentHash() != parent->second.hash){
		cdebug << "remove_parent_unmatch: blockNumber=" << blockNumber << ",parent->second.hash=" << parent->second.hash << ",header.parentHash()=" << header.parentHash();
		if(parent->second.time > uptime){
			cdebug << "clear_all:parent->second.time=" << parent->second.time << ",uptime=" << uptime << ",underHash()=" << underHash();
			clear();
			return;
		}else{
			removeUnder(blockNumber+1);
		}
	}

	cdebug << "insert_normal: blockNumber=" << blockNumber << ",_blocker.hash=" << _blocker.hash;
	insertBlocker(blockNumber, move(_blocker), header);
}

void Request::removeHeader(const bytes& _head)
{
	BlockHeader header(_head, HeaderData);
	if (header.transactionsRoot() != EmptyTrie || header.sha3Uncles() != EmptyListSHA3){
		Header headerId { header.transactionsRoot(), header.sha3Uncles() };
		auto iter = m_HeaderToNumber.find(headerId);
	
		cdebug << "header.number()=" << header.number() << "header.transactionsRoot()=" << header.transactionsRoot() << ",header.sha3Uncles()=" << header.sha3Uncles();

		if(iter != m_HeaderToNumber.end()){
			cdebug << "remove_header:header.number()=" << header.number() << "header.transactionsRoot()=" << header.transactionsRoot() << ",header.sha3Uncles()=" << header.sha3Uncles();
			m_HeaderToNumber.erase(iter);
		}
	}
}

/*
remove number < _blockNumber items
*/
void Request::removeUnder(unsigned _blockNumber)
{
	for(auto it = m_blocks.begin(); it != m_blocks.end();){
		if(it->first >= _blockNumber){
			cdebug << "it->first=" << it->first << ",_blockNumber=" << _blockNumber << ",m_blocks.size()=" << m_blocks.size();
			return;
		}

		if(0 == it->first){
			it = m_blocks.erase(it);
			continue;
		}
		
		removeHeader(it->second.head);

		cdebug << "remove:it->first=" << it->first;
		it = m_blocks.erase(it);
	}
}

/*
remove number < _blockNumber items
*/
void Request::removeUpWith(unsigned _blockNumber)
{
	for(auto it = m_blocks.find(_blockNumber); it != m_blocks.end(); _blockNumber++){
		if(it->first != _blockNumber){
			cdebug << "it->first=" << it->first << ",_blockNumber=" << _blockNumber << ",m_blocks.size()=" << m_blocks.size();
			return;
		}

		if(0 == it->first){
			it = m_blocks.erase(it);
			continue;
		}

		removeHeader(it->second.head);

		cdebug << "remove:it->first=" << it->first;
		it = m_blocks.erase(it);
	}
}

/*
remove number < _blockNumber items
*/
void Request::removeWith(unsigned _blockNumber)
{
	removeUnder(_blockNumber);
	removeUpWith(_blockNumber);
}

NodeID Request::id(unsigned _number)
{
	if(_number){
		auto it = m_blocks.find(_number);
		if(it != m_blocks.end())
			return it->second.id;

		return NodeID();
	}

	if(m_blocks.empty())
		return NodeID();

	auto it = m_blocks.end();
	--it;

	return it->second.id;
}

uint64_t Request::upTime() const
{
	if(m_blocks.empty())
		return 0;

	auto it = m_blocks.end();
	--it;

	return it->second.time;
}

h256 Request::underHash()
{
	auto it = m_blocks.begin();
	if(it != m_blocks.end())
		return it->second.hash;

	return h256(0);
}

unsigned Request::upNumber() const
{
	if(m_blocks.empty())
		return 0;

	auto it = m_blocks.end();
	--it;

	return it->first;
}

unsigned Request::underNumber() const
{
	auto it = m_blocks.begin();
	if(it != m_blocks.end())
		return it->first;

	return 0;
}

void Request::insertBlock(bytesConstRef _block, NodeID _id)
{
	try{
		insertBlocker(Blocker(_block, blockerBlock, _id));
	}catch(...){
		cdebug << "catch err:_block=" << _block.toBytes() << ",_id=" << _id;
	}
}

void Request::insertHead(bytesConstRef _block, NodeID _id)
{
	try{
		Blocker blocker(_block, blockerHead, _id);
		h256 hash = blocker.hash;
		insertBlocker(move(blocker));
		//assert(0 == m_heads.count(hash));
		m_heads.insert(hash);
	}catch(...){
		cdebug << "catch err:_block=" << _block.toBytes() << ",_id=" << _id;
	}
}

void Request::insertBody(bytesConstRef _body)
{
	RLP body(_body);
	
	auto txList = body[0];
	h256 transactionRoot = trieRootOver(txList.itemCount(), [&](unsigned i){ return rlp(i); }, [&](unsigned i){ return txList[i].data().toBytes(); });
	h256 uncles = sha3(body[1].data());
	Header id { transactionRoot, uncles };
	auto iter = m_HeaderToNumber.find(id);
	if (iter == m_HeaderToNumber.end())
	{
		clog(NetAllDetail) << "Ignored unknown block body";
		return;
	}
	
	unsigned blockNumber = iter->second;
	auto it = m_blocks.find(blockNumber);

	cdebug << "blockNumber=" << blockNumber << ",it->second.hash=" << it->second.hash << ",transactionRoot=" << transactionRoot << ",uncles=" << uncles;
	//assert(it != m_blocks.end() && it->second.body.empty());
	it->second.body = _body.toBytes();
}

h256s Request::requestBodys(unsigned _limit)
{
	h256s bodys;

	if(m_blocks.empty())
		return bodys;

	uint64_t now = utcTime();
	auto it = m_blocks.begin();
	unsigned number = it->first;
	for(; it != m_blocks.end(); ++it, ++number){
		if(bodys.size() >= _limit || number != it->first)
			return bodys;

		//assert(now > it->second.bodyTimeout);
		if(/*now < it->second.bodyTimeout || */!it->second.body.empty())
			continue;

		it->second.bodyTimeout = now + REQUEST_BODY_TIMEOUT;
		h256 hash = it->second.hash;
		bodys.push_back(hash);
	}

	return bodys;
}

std::tuple<unsigned, unsigned> Request::requestHeads()
{
	if(m_blocks.empty())
		return make_tuple(0, 0);
	
	auto it = m_blocks.begin();
	unsigned number = it->first;
	for(++it, ++number; it != m_blocks.end(); ++it, ++number){
		if(number != it->first)
			break;
	}

	if(it == m_blocks.end())
		return make_tuple(0, m_blocks.begin()->first);

	assert(it->first > number);
	return make_tuple(number, it->first-number);
}

bool Request::haveItem(unsigned blockNumber)
{
	return m_blocks.find(blockNumber) != m_blocks.end();
}

bool Request::empty()
{
	return m_blocks.empty();
}

unsigned Request::size()
{
	return m_blocks.size();
}

h256 Request::hash(unsigned _number)
{
	auto it = m_blocks.find(_number);
	if(it == m_blocks.end())
		return h256(0);

	assert(it->first == _number);
	return it->second.hash;
}

void Request::clearNeedless(class BlockChainRequest *request)
{
	if(empty())
		return;

	auto it = m_blocks.begin();
	unsigned number = it->first;
	for(; it != m_blocks.end(); ++it){
		if(!request->knowHash(it->second.hash)){
			cdebug << "it->second.hash=" << it->second.hash << ",number=" << number;
			break;
		}

		number = it->first;
	}

	if(number > 1)
		removeUnder(number);
}

void Request::collectBlocks(class BlockChainRequest *request)
{
	clearNeedless(request);
	
	auto it = m_blocks.begin();
	if(it == m_blocks.end())
		return;
	if(0 == it->first)
		++it;
	
	if(it->second.body.empty())
		++it;

	unsigned blockNumber = it->first;
	unsigned maxNumber = 0;
	for(; it != m_blocks.end(); ++it, ++blockNumber)
	{
		Blocker &blocker = it->second;
		if(blocker.body.empty() || blocker.head.empty()){
			cdebug << "it->first=" << it->first << ",blockNumber=" << blockNumber << ",blocker.hash=" << blocker.hash;
			break;
		}

		cdebug << "it->first=" << it->first << ",blocker.hash=" << blocker.hash;
		BlockHeader header(blocker.head, HeaderData);
		unsigned number = header.number();
		assert(number == it->first);
		
		RLP body(blocker.body);
		RLPStream blockStream(body.itemCount()+1);
		blockStream.appendRaw(blocker.head);

		for(size_t i = 0; i < body.itemCount(); i++)
			blockStream.appendRaw(body[i].data());

		bytes block;
		blockStream.swapOut(block);
		ImportResult result;

		try{
			result = request->bq().import(&block);
		}catch (Exception const&)
		{
			clog(NetWarn) << "Peer causing an Exception:" << boost::current_exception_diagnostic_information() << ",block=" << block;
			assert(false);
		}
		catch (std::exception const& _e)
		{
			clog(NetWarn) << "Peer causing an exception:" << _e.what() << ",block=" << block;
			assert(false);
		}
		
		cdebug << "result=" << (int)result << ",number=" << number << ",maxNumber=" << maxNumber;
		//assert(result != ImportResult::UnknownParent);
		switch (result)
		{
			case ImportResult::AlreadyKnown:
				{
					QueueStatus status = request->bq().blockStatus(header.hash());
					cdebug << "UnknownParent:status=" << (int)status << ",header.hash()=" << header.hash() << ",header.parentHash()=" << header.parentHash() << ",header.number()=" << header.number();
					if(status != QueueStatus::Ready && status != QueueStatus::Importing){
						if(status == QueueStatus::UnknownParent && it->first != blockNumber){
							assert(!request->knowHash(header.hash()));
							if(number == upNumber()){
								cdebug << "UnknownParent_upNumber:blocker.time=" << blocker.time << ",upTime()=" << upTime() << ",blocker.hash=" << blocker.hash << ",upNumber()=" << upNumber();
								return;
							}
							if(blocker.time >= upTime()){
								clear();
								cdebug << "UnknownParent_time:blocker.time=" << blocker.time << ",upTime()=" << upTime() << ",blocker.hash=" << blocker.hash;
								//return;
							}else{
								cdebug << "UnknownParent_time_old:blocker.time=" << blocker.time << ",upTime()=" << upTime() << ",blocker.hash=" << blocker.hash;
								removeUnder(number+1);
							}
							
							return;
							//assert(false);
						}
						goto end;
					}
				}
			case ImportResult::AlreadyInChain:
			case ImportResult::Success:
				if(maxNumber < number)
					maxNumber = number;
				break; 
			case ImportResult::Malformed:
				assert(false);
			case ImportResult::BadChain:
			case ImportResult::FutureTimeUnknown:
			case ImportResult::UnknownParent:
			case ImportResult::FutureTimeKnown:
				goto end;
			default:;
		}
	}

end:
	cdebug << "maxNumber=" << maxNumber << ",upNumber()=" << upNumber();
	if(0 == maxNumber)
		return;

	if(maxNumber >= upNumber()){
		clear();
	}else{
		removeUnder(maxNumber);
	}

}

BlockChainRequest::BlockChainRequest(EthereumHost& _host):
	m_host(_host), m_request(_host.chain().genesisHash())
{
	host().bq().onRoomAvailable([this]()
	{
		RecursiveGuard l(x_sync);
		host().foreachPeer([this](std::shared_ptr<EthereumPeer> _p)
		{
			syncPeer(_p, false);
			return true;
		});
	});
}

SyncStatus BlockChainRequest::status() const
{
	RecursiveGuard l(x_sync);
	SyncStatus res;
	res.state = SyncState::Idle;
	res.protocolVersion = 62;
	res.startBlockNumber = m_request.underNumber() < 1 ? 1 : m_request.underNumber();
	res.currentBlockNumber = host().chain().number();
	res.highestBlockNumber = m_highestBlock;
	return res;
}

void BlockChainRequest::syncPeer(std::shared_ptr<EthereumPeer> _peer, bool _force)
{
	u256 td = host().chain().details().totalDifficulty;
	if (host().bq().isActive())
		td += host().bq().difficulty();

	u256 syncingDifficulty = std::max(m_syncingTotalDifficulty, td);
	cdebug << "_force=" << _force << ",_peer->m_totalDifficulty=" << _peer->m_totalDifficulty << ",syncingDifficulty=" << syncingDifficulty;
	if (_force || _peer->m_totalDifficulty > syncingDifficulty)
	{
		// start sync
		m_syncingTotalDifficulty = _peer->m_totalDifficulty;
		_peer->requestBlockHeaders(_peer->m_latestHash, 1, 0, false);
		_peer->m_requireTransactions = true;
		cdebug << "_peer->m_latestHash=" << _peer->m_latestHash;
		return;
	}
}

void BlockChainRequest::onBlockImported(BlockHeader const& _info)
{
	//if a block has been added via mining or other block import function
	//through RPC, then we should count it as a last imported block
	RecursiveGuard l(x_sync);
	unsigned number = static_cast<unsigned>(_info.number());
	unsigned upnumber = m_request.upNumber();
	cdebug << "number=" << number << ",m_request.hash(number)=" << m_request.hash(number) << ",_info.hash()" << _info.hash() << ",upnumber=" << upnumber;
	if(number > upnumber + 20){
		m_request.clear();
		return;
	}
	
	if(_info.hash() != m_request.hash(number))
		return;

	m_request.removeUnder(number);
}

void BlockChainRequest::continueSync(std::shared_ptr<EthereumPeer> _peer)
{
	m_request.collectBlocks(this);
	if(m_request.empty() || _peer->id() != m_request.id()){
		cdebug << "not_continueSync:_peer->id()=" << _peer->id() << ",m_request.id()=" << m_request.id() << ",m_request.empty()=" << m_request.empty();
		return;
	}

	unsigned start = m_request.underNumber();
	unsigned count = 0;
	h256 startHash = m_request.underHash();
	unsigned mystart = static_cast<unsigned>(m_host.chain().number());
	if(knowHash(startHash)){
		h256s bodys = m_request.requestBodys(MAX_REQUEST_COUNT);
		if (!bodys.empty()){
			_peer->requestBlockBodies(bodys);
			cdebug << "request_body:start=" << start << ",bodys.size()=" << bodys.size();
			return;
		}

		unsigned end = start;
		tie(start, count) = m_request.requestHeads();
		cdebug << "start=" << start << ",count=" << count << ",mystart=" << mystart << ",end=" << end;
		end = start + count;
		if(mystart > end && count > MAX_REQUEST_COUNT*4){
			start = start + count/2;
			count = end - start;
		}
		
		if(count > MAX_REQUEST_COUNT)
			count = MAX_REQUEST_COUNT;

		//assert(start > 0 && count > 0);
		if(start > 0 && count > 0)
			_peer->requestBlockHeaders(start, count, 0, false);
		
		cdebug << "request_head_konw:start=" << start << ",count=" << count;
		return;
	}

	cdebug << "start=" << start << ",count=" << count << ",mystart=" << mystart << ",startHash=" << startHash;
	assert(start);
	static_assert(MAX_REQUEST_COUNT > 2, "MAX_REQUEST_COUNT too small");
	if(start > mystart && mystart > MAX_REQUEST_COUNT){
		mystart -= MAX_REQUEST_COUNT/2;
		count = start - mystart;
		start = mystart;
	}else if(start > 1){
		count = start - (start>>1);
		start = start>>1;
	}
	
	assert(count > 0);
	if(count > MAX_REQUEST_COUNT)
		count = MAX_REQUEST_COUNT;
	
	_peer->requestBlockHeaders(start, count, 0, false);
	cdebug << "request_head_unkonw:start=" << start << ",count=" << count;
}

void BlockChainRequest::onPeerStatus(std::shared_ptr<EthereumPeer> _peer)
{
	RecursiveGuard l(x_sync);

	std::shared_ptr<SessionFace> session = _peer->session();
	if (!session)
		return; // Expired
	if (_peer->m_genesisHash != host().chain().genesisHash())
		_peer->disable("Invalid genesis hash");
	else if (_peer->m_protocolVersion != host().protocolVersion() && _peer->m_protocolVersion != EthereumHost::c_oldProtocolVersion)
		_peer->disable("Invalid protocol version.");
	else if (_peer->m_networkId != host().networkId())
		_peer->disable("Invalid network identifier.");
	else if (session->info().clientVersion.find("/v0.7.0/") != string::npos)
		_peer->disable("Blacklisted client version.");
	else if (host().isBanned(session->id()))
		_peer->disable("Peer banned for previous bad behaviour.");
	else if (_peer->m_asking != Asking::State && _peer->m_asking != Asking::Nothing)
		_peer->disable("Peer banned for unexpected status message.");
	else{
		cdebug << "m_request.size()=" << m_request.size();
		syncPeer(_peer, false);
		continueSync(_peer);
	}
}

bool BlockChainRequest::knowHash(h256 _hash)
{
	if(_hash == h256(0))
		return false;
	
	auto status = host().bq().blockStatus(_hash);
	return status == QueueStatus::Importing || status == QueueStatus::Ready || host().chain().isKnown(_hash);
}

void BlockChainRequest::onPeerBlockBodies(std::shared_ptr<EthereumPeer> _peer, RLP const& _r)
{
	RecursiveGuard l(x_sync);

	size_t itemCount = _r.itemCount();
	clog(NetMessageSummary) << "BlocksBodies (" << dec << itemCount << "entries)" << (itemCount ? "" : ": NoMoreBodies");

	if (itemCount == 0)
	{
		clog(NetAllDetail) << "Peer does not have the blocks requested";
		_peer->addRating(-1);
	}
	for (unsigned i = 0; i < itemCount; i++)
	{
		m_request.insertBody(_r[i].data());
	}
	
	continueSync(_peer);
}

void BlockChainRequest::onPeerBlockHeaders(std::shared_ptr<EthereumPeer> _peer, RLP const& _r)
{
	RecursiveGuard l(x_sync);

	size_t itemCount = _r.itemCount();
	clog(NetMessageSummary) << "BlocksHeaders (" << dec << itemCount << "entries)" << (itemCount ? "" : ": NoMoreHeaders") << ",_peer->id()=" << _peer->id();

	if (itemCount == 0)
	{
		clog(NetAllDetail) << "Peer does not have the blocks requested";
		_peer->addRating(-1);
	}

	u256 td = host().chain().details().totalDifficulty;
	bool diffculty = _peer->m_totalDifficulty > td;
	unsigned myNumber = static_cast<unsigned>(m_host.chain().number());
	unsigned maxNumber = 0;
	int index = -1;
	for (unsigned i = 0; i < itemCount; i++)
	{
		BlockHeader info(_r[i].data(), HeaderData);
		unsigned blockNumber = static_cast<unsigned>(info.number());
		cdebug << "blockNumber=" << blockNumber;
		if(m_highestBlock < blockNumber)
			m_highestBlock = blockNumber;
		if (m_request.haveItem(blockNumber) && m_request.id(blockNumber) == m_request.id())
		{
			clog(NetMessageSummary) << "Skipping_header: blockNumber=" << blockNumber << ",m_request.id(blockNumber)=" << m_request.id(blockNumber) << ",m_request.id()=" << m_request.id();
			continue;
		}

		cdebug << "m_request.empty()=" << m_request.empty() << ",maxNumber=" << maxNumber << ",blockNumber=" << blockNumber << ",knowHash(info.hash())=" << knowHash(info.hash()) << ",diffculty=" << diffculty << ",myNumber=" << myNumber << ",info.difficulty()=" << info.difficulty();
		if(!m_request.empty()){
			//conitnue
			m_request.insertHead(_r[i].data(), _peer->id());
		}else{
			//begin
			cdebug << "start_request:maxNumber=" << maxNumber << ",blockNumber=" << blockNumber << ",knowHash(info.hash())=" << knowHash(info.hash()) << ",diffculty=" << diffculty << ",myNumber=" << myNumber << ",info.difficulty()=" << info.difficulty();
			if(diffculty || myNumber < blockNumber)
				m_request.insertHead(_r[i].data(), _peer->id());
		}
		
		if (knowHash(info.hash())){
			if(blockNumber > maxNumber){
				maxNumber = blockNumber;
				index = (int)i;
			}
		}
	}

	if(maxNumber > m_request.underNumber() || (!knowHash(m_request.underHash()) && maxNumber > 0)){
		cdebug << "insert konw maxNumber=" << maxNumber << ",index=" << index;
		if(maxNumber > m_request.underNumber())
			m_request.removeUnder(maxNumber);
		m_request.insertHead(_r[index].data(), _peer->id());
	}

	continueSync(_peer);
}

void BlockChainRequest::onPeerNewBlock(std::shared_ptr<EthereumPeer> _peer, RLP const& _r)
{
	RecursiveGuard l(x_sync);

	if (_r.itemCount() != 2)
	{
		_peer->disable("NewBlock without 2 data fields.");
		return;
	}
	BlockHeader info(_r[0][0].data(), HeaderData);
	auto h = info.hash();
	DEV_GUARDED(_peer->x_knownBlocks)
		_peer->m_knownBlocks.insert(h);
	unsigned blockNumber = static_cast<unsigned>(info.number());
	if(blockNumber > m_highestBlock)
		m_highestBlock = blockNumber;

	ImportResult result = host().bq().import(_r[0].data());
	cdebug << "m_highestBlock=" << m_highestBlock << ",blockNumber=" << blockNumber << ",result=" << (int)result << ",info.difficulty()=" << info.difficulty();
	switch (result)
	{
	case ImportResult::Success:
		_peer->addRating(100);
		//m_request.removeUnder(blockNumber);
		break;
	case ImportResult::FutureTimeKnown:
		//TODO: Rating dependent on how far in future it is.
		break;

	case ImportResult::Malformed:
	case ImportResult::BadChain:
		_peer->disable("Malformed block received.");
		return;

	case ImportResult::AlreadyInChain:
	case ImportResult::AlreadyKnown:
		//m_request.removeUnder(blockNumber);
		break;
	case ImportResult::FutureTimeUnknown:
	case ImportResult::UnknownParent:
	{
		if(!m_request.empty()){
			//conitnue
			m_request.insertBlock(_r[0].data(), _peer->id());
			cdebug << "conitnue_request:blockNumber=" << blockNumber << ",knowHash(info.hash())=" << knowHash(info.hash()) << ",info.difficulty()=" << info.difficulty();
		}else{
			u256 td = host().chain().details().totalDifficulty;
			bool diffculty = _peer->m_totalDifficulty > td;
			unsigned myNumber = static_cast<unsigned>(m_host.chain().number());
			cdebug << "start_request:blockNumber=" << blockNumber << ",knowHash(info.hash())=" << knowHash(info.hash()) << ",diffculty=" << diffculty << ",myNumber=" << myNumber << ",info.difficulty()=" << info.difficulty();
			//begin
			if(diffculty || myNumber < blockNumber){
				m_request.insertBlock(_r[0].data(), _peer->id());
				continueSync(_peer);
			}
		}
		break;
	}
	default:;
	}
}

void BlockChainRequest::onPeerNewHashes(std::shared_ptr<EthereumPeer> _peer, std::vector<std::pair<h256, u256>> const& _hashes)
{
	RecursiveGuard l(x_sync);
	if (_peer->isConversing())
	{
		clog(NetMessageDetail) << "Ignoring new hashes since we're already downloading.";
		return;
	}
	clog(NetMessageDetail) << "Not syncing and new block hash discovered: syncing.";
	unsigned knowns = 0;
	unsigned unknowns = 0;
	unsigned maxHeight = 0;
	for (auto const& p: _hashes)
	{
		h256 const& h = p.first;
		_peer->addRating(1);
		DEV_GUARDED(_peer->x_knownBlocks)
			_peer->m_knownBlocks.insert(h);
		auto status = host().bq().blockStatus(h);
		if (knowHash(h))
			knowns++;
		else if (status == QueueStatus::Bad)
		{
			cwarn << "block hash bad!" << h << ". Bailing...";
			return;
		}
		else if (status == QueueStatus::Unknown)
		{
			unknowns++;
			if (p.second > maxHeight)
			{
				maxHeight = (unsigned)p.second;
				_peer->m_latestHash = h;
			}
		}
		else
			knowns++;
	}
	clog(NetMessageSummary) << knowns << "knowns," << unknowns << "unknowns";
	if (unknowns > 0)
	{
		clog(NetMessageDetail) << "Not syncing and new block hash discovered: syncing.";
		syncPeer(_peer, true);
	}
}


