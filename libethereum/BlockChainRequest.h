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
/** @file BlockChainRequest.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <set>

#include <mutex>
#include <unordered_map>

#include <libdevcore/Guards.h>
#include <libethcore/Common.h>
#include <libethcore/BlockHeader.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>
#include <libp2p/Common.h>
#include "TransactionQueue.h"
#include "CommonNet.h"

#include "EthereumHost.h"

namespace dev
{

class RLPStream;

namespace eth
{

class EthereumHost;
class BlockQueue;
class EthereumPeer;

enum 
{
	blockerHead,
	blockerBlock
};

class Blocker
{
public:
	Blocker(h256 _hash){hash = _hash;};
	Blocker(){time = utcTime();};
	Blocker(bytesConstRef _block, unsigned _type, NodeID _id);
	Blocker& operator=(Blocker&& _other)
	{
		assert(&_other != this);

		//transaction = std::move(_other.transaction);
		head = std::move(_other.head);
		body = std::move(_other.body);
		bodyTimeout = std::move(_other.bodyTimeout);
		hash = std::move(_other.hash);
		parent = std::move(_other.parent);
		id = std::move(_other.id);
		time = _other.time;
		return *this;
	}
	Blocker& operator=(Blocker& _other)
	{
		assert(&_other != this);

		//transaction = std::move(_other.transaction);
		head = _other.head;
		body = _other.body;
		bodyTimeout = _other.bodyTimeout;
		hash = _other.hash;
		parent = _other.parent;
		id = _other.id;
		time = _other.time;
		return *this;
	}
	
	bytes head;		///< Header data
	bytes body;		///< Header data
	uint64_t bodyTimeout = 0;		///< Header data
	h256 hash;		///< Block hash
	h256 parent;	///< Parent hash
	NodeID id;
	uint64_t time;
};

struct Header
{
	h256 transactionsRoot;
	h256 uncles;

	bool operator<(Header const& _other) const
	{
		return transactionsRoot < _other.transactionsRoot || (transactionsRoot == _other.transactionsRoot && uncles < _other.uncles);
	}
};

class BlockChainRequest;

class Request
{
public:
	Request(h256 _hash):m_genesis(_hash){};
	void insertBlocker(unsigned blockNumber, Blocker&& _block, const BlockHeader& _header);
	void insertBlocker(Blocker&& _blocker);
	void insertBlock(bytesConstRef _block, NodeID _id);
	void insertHead(bytesConstRef _block, NodeID _id);
	void insertBody(bytesConstRef _block);

	void clear();
	h256 underHash();
	uint64_t upTime() const;
	unsigned upNumber() const;
	unsigned underNumber() const;
	void removeWith(unsigned _blockNumber);
	void removeHeader(const bytes& _head);
	void removeUpWith(unsigned _blockNumber);
	void removeUnder(unsigned blockNumber);

	dev::h256s requestBodys(unsigned _limit);
	std::tuple<unsigned, unsigned>  requestHeads();

	bool haveItem(unsigned blockNumber);
	bool empty();
	unsigned size();
	h256 hash(unsigned _number);
	void collectBlocks(class BlockChainRequest *request);

	NodeID id(unsigned _number = 0);
protected:
	void clearNeedless(class BlockChainRequest *request);
	
private:
	std::map<unsigned, Blocker> m_blocks;
	std::map<Header, unsigned> m_HeaderToNumber;
	Blocker m_genesis;
	std::set<h256> m_heads;
};

class BlockChainRequest: public BlockChainSyncInterface
{
public:
	BlockChainRequest(EthereumHost& _host);
	void restartSync(){};
	SyncStatus status() const;
	void continueSync(std::shared_ptr<EthereumPeer> _peer);
	void onPeerStatus(std::shared_ptr<EthereumPeer> _peer);

	/// Called by peer once it has new block headers during sync
	void onPeerBlockHeaders(std::shared_ptr<EthereumPeer> _peer, RLP const& _r);

	/// Called by peer once it has new block bodies
	void onPeerBlockBodies(std::shared_ptr<EthereumPeer> _peer, RLP const& _r);

	/// Called by peer once it has new block bodies
	void onPeerNewBlock(std::shared_ptr<EthereumPeer> _peer, RLP const& _r);

	void onPeerNewHashes(std::shared_ptr<EthereumPeer> _peer, std::vector<std::pair<h256, u256>> const& _hashes);

	/// Called by peer when it is disconnecting
	void onPeerAborting(){};

	/// Called when a blockchain has imported a new block onto the DB
	void onBlockImported(BlockHeader const& _info);
	BlockQueue& bq() { return host().bq(); }
	BlockChain const& chain() const { return host().chain(); }
	bool knowHash(h256 _hash);
private:
	void syncPeer(std::shared_ptr<EthereumPeer> _peer, bool _force);
	
private:
	EthereumHost& host() { return m_host; }
	EthereumHost const& host() const { return m_host; }

	EthereumHost& m_host;
	Request m_request;

	mutable RecursiveMutex x_sync;

	u256 m_syncingTotalDifficulty;				///< Highest peer difficulty
	unsigned m_highestBlock = 0;
};

}
}
