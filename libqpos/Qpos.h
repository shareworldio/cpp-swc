
#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <set>
#include <map>

#include <libethereum/ChainParams.h>
#include <libethereum/Client.h>
#include <libdevcore/concurrent_queue.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>


#include "QposSealEngine.h"

using namespace std;
using namespace dev;
using namespace eth;

namespace dev
{
namespace eth
{

enum qposState
{
	qposInitial,
	qposWaitingVote,
	qposFinished
};

class Qpos:public QposSealEngine
{
public:
	virtual bool interpret(QposPeer*, unsigned _id, RLP const& _r);

	virtual bool shouldSeal(Interface*);

	virtual void initEnv(class Client *_c, p2p::Host *_host, BlockChain* _bc, bool _importAnyNode) override;
	virtual void generateSeal(bytes const& _block) override;
	void generateSealBegin(bytes const& _block);

	//void generateSeal(BlockHeader const& _bi, bytes const& _block_data) override;
	void onSealGenerated(std::function<void(bytes const& _block, bool _isOurs)> const& _f)  { m_onSealGenerated = _f;}

	void reportBlock(unsigned _blockNumber, bool _force = false);
	h512s getMinerNodeList();
	int64_t lastConsensusTime() const { /*Guard l(m_mutex);*/ return m_last_consensus_time;};

	bool checkBlockSign(BlockHeader const& _header, bytesConstRef _block) const override;
protected:
	virtual void tick();
	void reportBlockSelf();
	
	void resetConfig();

	void onBlockVoteAck(QposPeer* _p, RLP const& _r);
	void onBlockVote(QposPeer* _p, RLP const& _r);
	void onHeart(QposPeer* _p, RLP const& _r);
	void onVote(QposPeer* _p, RLP const& _r);
	void onVoteAck(QposPeer* _p, RLP const& _r);
	void voteTick();
	bytes authBytes();
	void broadBlock(bytes const& _bolck);
	void onBroadBlock(QposPeer* _p, RLP const& _r);
	void voteBlockEnd();
	void voteBlockBegin();
	bool msgVerify(const NodeID &_nodeID, bytes const&  _msg, h520 const&  _msgSign);
	void addNodes(const std::string &_nodes);
	int64_t nodeCount() const;
	void signalHandler(const boost::system::error_code& err, int signal);
	
private:
	unsigned m_consensusTimeInterval = (20*TIME_PER_SECOND);

	std::function<void(bytes const& _block, bool _isOurs)> m_onSealGenerated;

	std::atomic<int64_t> m_blockReport = {0};
	int64_t m_blockNumber = 0;
	int64_t m_blockNumberRecv = 0;
	set<NodeID> m_miners;
	
	bytes m_blockBytes;
	int64_t m_currentView = 0;
	int64_t m_consensusTimeOut = -1;
	set<NodeID> m_idUnVoted;
	map<NodeID, Signature> m_idVoted;
	//unsigned m_consensusState = qposInitial;
	std::atomic<unsigned> m_consensusState = { qposFinished };
	
	std::atomic<int64_t> m_last_consensus_time = {0};

	concurrent_queue<bytes> m_blocks;

	std::atomic<pid_t> m_hostTid = {0};
	boost::asio::signal_set *m_sigset;

	int64_t m_voteTimeOut = 0;
	set<NodeID> m_voted;
	std::atomic<bool> m_isLeader = { false };
	bool m_importAnyNode = false;
};


}
}

