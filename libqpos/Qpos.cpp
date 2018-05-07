
//#include <boost/algorithm/string.hpp>


#include "Common.h"
#include "Qpos.h"

#include <sys/syscall.h>  

#define gettidv1() syscall(__NR_gettid)  
#define gettidv2() syscall(SYS_gettid) 

#define QPOS_SIGNAL SIGUSR1
#define QPOS_SIGNAL_REPORT SIGUSR2

#define QPOS_VOTE_TIMEOUT (10*TIME_PER_SECOND)
#define QPOS_HEART_TIMEOUT (2*TIME_PER_SECOND)

using namespace dev;
using namespace dev::eth;

using namespace boost::asio;
using namespace ba::ip;

void Qpos::tick()
{
	voteTick();
	if(!m_isLeader)
		return;
	
	int64_t now = utcTime();
	if(qposInitial == m_consensusState){
		bool have;
		bytes blockBytes;
		tie(have, blockBytes) = m_blocks.tryPop(0);

		if(have && !blockBytes.empty())
			generateSealBegin(blockBytes);
	}else{
		if(now > m_consensusTimeOut){
			cdebug << "m_idUnVoted.size()=" << m_idUnVoted.size() << ",m_idVoted.size()=" << m_idVoted.size() << ",nodeCount()=" << nodeCount();

			resetConfig();
		}
	}
}

bool Qpos::interpret(QposPeer* _p, unsigned _id, RLP const& _r)
{
	unsigned msgType;
	try{
		msgType = _r[0][0].toInt();
	}catch(...){
		cdebug << "msgType err _id= " << _id << ",_p->id()=" << _p->id();
		return true;
	}

	bool contain = m_miners.count(_p->id());
	if(!contain){
		cdebug << "interpret _p->id()=" << _p->id() << ",_id=" << _id << ",msgType=" << msgType;
		return true;
	}

	cdebug << "interpret _p->id()=" << _p->id() << ",_id=" << _id << ",msgType=" << msgType << ",m_isLeader=" << (bool)m_isLeader;
	try{
		switch(msgType){
			case qposBlockVote:
				onBlockVote(_p, _r);
				break;
			case qposBlockVoteAck:
				onBlockVoteAck(_p, _r);
				break;
			case qposHeart:
				onHeart(_p, _r);
				break;
			case qposVote:
				onVote(_p, _r);
				break;
			case qposVoteAck:
				onVoteAck(_p, _r);
				break;
			case qposBroadBlock:
				onBroadBlock(_p, _r);
				break;
			default:
				break;
		}
	}catch(...){
		cwarn << "catchErrinterpret _p->id()=" << _p->id() << ",_id=" << _id << ",msgType=" << msgType;
	}

	return true;
}

void Qpos::reportBlock(unsigned _blockNumber, bool _force)
{
	if(_blockNumber > m_blockReport || _force){
		m_blockReport = _blockNumber;
		kill(m_hostTid, QPOS_SIGNAL_REPORT);
	}

	cdebug << "_blockNumber=" << _blockNumber << ",m_blockNumber=" << m_blockNumber << ",_force=" << _force;
}

void Qpos::reportBlockSelf()
{
	unsigned long long blockNumber = m_blockReport;

	cdebug << "blockNumber=" << blockNumber << ",m_blockNumber=" << m_blockNumber;

	m_blockNumber = blockNumber;
	m_blockNumberRecv = blockNumber;

	assert(getNodes(m_miners));

	if(m_miners.empty() || (1 == m_miners.size() && m_miners.count(id())) ){
		m_isLeader = true;
	}else if(!m_miners.count(id()) && m_isLeader){
		m_isLeader = false;
	}

	resetConfig();
}

void Qpos::resetConfig()
{
	m_blockBytes = bytes();
	//m_currentView = 0;
	m_consensusTimeOut = 0;
	m_idVoted.clear();
	m_idUnVoted.clear();
	m_last_consensus_time = utcTime();
	m_consensusState = qposInitial;

	cdebug << "m_consensusState=" << m_consensusState << ",nodeCount()=" << nodeCount();
	cdebug << ",id()=" << id() << ",m_currentView=" << (unsigned)m_currentView << ",m_miners.size()=" << m_miners.size() << ",m_isLeader=" << m_isLeader;
}

void Qpos::signalHandler(const boost::system::error_code& err, int signal)
{
	cdebug << "_host.onTick err = " << err << ",signal = " << signal << ",m_hostTid=" << m_hostTid;

	if(QPOS_SIGNAL == signal){
		tick();
	}else if(QPOS_SIGNAL_REPORT == signal){
		reportBlockSelf();
	}
	m_sigset->async_wait(boost::bind(&Qpos::signalHandler, this, _1, _2));
}

void Qpos::initEnv(class Client *_c, p2p::Host *_host, BlockChain* _bc)
{
	m_sigset = new signal_set(_host->ioService(), QPOS_SIGNAL, QPOS_SIGNAL_REPORT);
	m_sigset->async_wait(boost::bind(&Qpos::signalHandler, this, _1, _2));

	DumpStack();
	
	srand(utcTime());
	QposSealEngine::initEnv(_c, _host, _bc);	

	_host->onTick(1000, [=](const boost::system::error_code& _e) {
		(void)_e;
		this->tick();
	});

	_host->onInit([=]() {
		m_hostTid = gettidv1();

		for(auto it : exNodes()){
			if(it.id() != id())
				_host->addPeer(it, p2p::PeerType::Required);
		}
		cdebug << "_host.onTick**************************************************************************************** m_hostTid =" << m_hostTid;
	});

	_c->onFilter([=](p2p::NodeID _nodeid, unsigned _id) -> bool{
		if(m_miners.empty())
			return true;
		
		bool contain = m_miners.count(_nodeid);
		cdebug << "_nodeid=" << _nodeid << ",_id=" << _id << ",contain=" << contain;
		if(contain)
			return true;

		switch(_id)
		{
			case BlockHeadersPacket:
			case BlockBodiesPacket:
			case NewBlockPacket:
			case NewBlockHashesPacket:
				return false;
		}
		
		return true;
	});
}

int64_t Qpos::nodeCount() const
{
	return m_miners.size();
}

bool Qpos::msgVerify(const NodeID &_nodeID, bytes const&  _msg, h520 const&  _msgSign)
{
	BlockHeader header(_msg);
	//h256 hash =  sha3(_msg);
	h256 hash =  header.hash(WithoutSeal);

	cdebug << "_nodeID=" << _nodeID << ",hash=" << hash << ",_msgSign=" << _msgSign;
	return dev::verify(_nodeID, _msgSign, hash);
}

bytes Qpos::authBytes()
{
	bytes ret;
	RLPStream authListStream;
	authListStream.appendList(m_idVoted.size()*2);

	for(auto it : m_idVoted){
		authListStream << it.first; 
		authListStream << it.second; 
	}

	authListStream.swapOut(ret);
	return ret;
}

void Qpos::broadBlock(bytes const& _bolck)
{
	assert(m_isLeader);
	if(_bolck.size()){
		RLPStream msg;
		msg << qposBroadBlock;
		msg << _bolck;

		broadcast(msg);
	}

	m_consensusState = qposFinished;
	m_onSealGenerated(_bolck, true);
	cdebug << "_bolck.size()=" << _bolck.size() << ",m_isLeader=" << m_isLeader;
}

void Qpos::onBroadBlock(QposPeer* _p, RLP const& _r)
{
	(void)_p;
	
	bytes blockBytes = _r[0][1].toBytes();
	BlockHeader header(blockBytes);
	int64_t number = header.number();
	cdebug << "blockBytes.size()=" << blockBytes.size() << ",number=" << number << ",m_blockNumber=" << m_blockNumber;
	if(number > m_blockNumber){
		m_blockNumber = number;
	}

	m_onSealGenerated(blockBytes, false);
}

void Qpos::voteBlockEnd()
{
	if((int64_t)m_idUnVoted.size() >= (nodeCount()+1)/2){
		cdebug << "Vote failed: m_idUnVoted.size()=" << m_idUnVoted.size() << ",m_idVoted.size()=" << m_idVoted.size() << ",nodeCount()=" << nodeCount();

		//broadBlock(bytes());
		resetConfig();
	}else if((int64_t)m_idVoted.size() > nodeCount()/2){
		cdebug << "Vote succed, m_blockBytes.size() = " << m_blockBytes.size();

		try{
			if(m_onSealGenerated){
				std::vector<std::pair<u256, Signature>> sig_list;

				size_t i = 0;
				for(auto it : m_miners){
					if(m_idVoted.count(it)){
						sig_list.push_back(std::make_pair(u256(i++), m_idVoted[it]));
					}
				}

				BlockHeader header(m_blockBytes);
				RLP r(m_blockBytes);
				RLPStream rs;
				rs.appendList(5);
				rs.appendRaw(r[0].data()); // header
				rs.appendRaw(r[1].data()); // tx
				rs.appendRaw(r[2].data()); // uncles
				rs.append(header.number()); // number
				rs.appendVector(sig_list); // sign_list

				bytes blockBytes;
				rs.swapOut(blockBytes);
				
				cdebug << "Vote_succed: idx.count blockBytes.size() = " << blockBytes.size() << ",header.number()=" << header.number() << ",header.hash()=" << header.hash(WithoutSeal);
				
				broadBlock(blockBytes);
			}
		}catch(...){
			cwarn << "m_consensusFinishedFunc run err";
		}
	}
}

void Qpos::onBlockVoteAck(QposPeer* _p, RLP const& _r)
{	
	if(m_consensusState != qposWaitingVote){
		cdebug << "m_consensusState=" << m_consensusState;
		return; 
	}
	
	bool vote = _r[0][1].toInt();	
	int64_t currentView = _r[0][2].toInt();
	Signature mySign = h520(_r[0][3].toBytes());
	NodeID idrecv(_r[0][4].toBytes());

	bool v = msgVerify(_p->id(), m_blockBytes, mySign);
	cdebug << ",v=" << v << ",currentView=" << currentView << ",nodeCount()=" << nodeCount() << ",vote=" << vote << ",m_currentView=" << m_currentView << ",m_blockNumber=" << m_blockNumber << ",idrecv=" << idrecv << ",m_consensusState=" << static_cast<unsigned>(m_consensusState);
	if(!v || currentView != m_currentView || qposFinished == m_consensusState)
		return;
	
	if(vote){
		m_idVoted[_p->id()] = mySign;
	}else{
		m_idUnVoted.insert(_p->id());
	}

	cdebug << ",m_idUnVoted.size()=" << m_idUnVoted.size() << ",m_idVoted.size()=" << m_idVoted.size() << ",nodeCount()=" << nodeCount();
	voteBlockEnd();
}

void Qpos::onBlockVote(QposPeer* _p, RLP const& _r)
{
	int64_t currentView = _r[0][1].toInt();
	Signature mySign = h520(_r[0][2].toBytes());
	bytes blockBytes = _r[0][3].toBytes();
	BlockHeader header(blockBytes);
	int64_t blockNumber = header.number();
	bool vote = false;
	int64_t now = utcTime();

	bool verify = msgVerify(_p->id(), blockBytes, mySign);
	bool verifyblock = true;
	//verifyblock = verifyBlock(blockBytes);

	cdebug << "verify =" << verify << ",verifyblock=" << verifyblock << ",nodeCount()=" << nodeCount() << ",currentView=" << currentView << ",m_currentView=" << m_currentView << ",blockNumber=" << blockNumber << ",m_blockNumber=" << m_blockNumber;
	if(verify && verifyblock && m_blockNumber + 1 == blockNumber && currentView == m_currentView){
		vote = true;
		m_idVoted.clear();
		m_idUnVoted.clear();
		m_consensusTimeOut = utcTime() + m_consensusTimeInterval;
		m_blockBytes = blockBytes;
		m_idVoted[_p->id()] = mySign;
		cdebug << "m_currentView =" << m_currentView << ",m_blockBytes.size()=" << m_blockBytes.size();
	}
	
	h256 hash =  sha3(blockBytes);
	mySign = sign(header.hash(WithoutSeal));

	if(vote)
		m_idVoted[id()] = mySign;
	
	RLPStream data;
	data << qposBlockVoteAck;
	data << (uint8_t)vote; 
	data << m_currentView; 
	data << mySign; 
	data << _p->id(); 

	m_voteTimeOut = now + QPOS_VOTE_TIMEOUT;

	send(_p->id(), data);
	cdebug << "hash=" << hash << ",mySign=" << mySign << ",blockBytes.size()=" << blockBytes.size() << ",vote=" << vote << ",_p->id()=" << _p->id();
}

void Qpos::voteTick()
{
	int64_t now = utcTime();
	if(m_voteTimeOut > now)
		return;

	if(m_isLeader){
		m_voteTimeOut = now + QPOS_HEART_TIMEOUT;

		RLPStream msg;
		msg << qposHeart;
		msg << m_currentView;

		multicast(m_miners, msg);
	}else{
		m_voteTimeOut = now + QPOS_VOTE_TIMEOUT;
		
		RLPStream msg;
		msg << qposVote;
		msg << ++m_currentView;
		msg << m_blockNumber;

		m_voted.clear();
		multicast(m_miners, msg);
	}

	//cdebug << "m_currentView=" << m_currentView << ",m_isLeader=" << m_isLeader << ",now=" << now;
}

void Qpos::onHeart(QposPeer* _p, RLP const& _r)
{
	int64_t currentView = _r[0][1].toInt();

	if(m_isLeader && (currentView > m_currentView ||  (currentView == m_currentView && _p->id() > id())))
		m_isLeader = false;

	m_voteTimeOut = utcTime() + QPOS_VOTE_TIMEOUT;
	cdebug << "currentView=" << currentView << ",m_isLeader=" << (bool)m_isLeader;
}

void Qpos::onVote(QposPeer* _p, RLP const& _r)
{
	int64_t currentView = _r[0][1].toInt();
	int64_t blockNumberRecv = _r[0][2].toInt();
	bool vote = currentView > m_currentView ? true : false;

	if(blockNumberRecv > m_blockNumberRecv)
		m_blockNumberRecv = blockNumberRecv;
	if(currentView > m_currentView && m_blockNumberRecv >= m_blockNumber){
		m_currentView = currentView;
		m_voteTimeOut = utcTime() + QPOS_VOTE_TIMEOUT + rand()%QPOS_HEART_TIMEOUT;
	}
	
	RLPStream data;
	data << qposVoteAck;
	data << vote; 

	cdebug << "_p->id()=" << _p->id() << ",currentView=" << currentView << ",m_currentView=" << m_currentView << ",vote=" << vote;
	send(_p->id(), data);
}

void Qpos::onVoteAck(QposPeer* _p, RLP const& _r)
{
	bool vote = _r[0][1].toInt();
	cdebug << "vote=" << vote << ",m_voted.size()=" << m_voted.size() << ",nodeCount()=" << nodeCount();
	if(!vote)
		return;
	
	m_voted.insert(_p->id());
	if((1+(int64_t)m_voted.size()) > nodeCount()/2)
		m_isLeader = true;
}

void Qpos::voteBlockBegin()
{
	BlockHeader header(m_blockBytes);
	Signature mySign = sign(header.hash(WithoutSeal));
	m_idVoted[id()] = mySign;

	m_consensusTimeOut = utcTime() + m_consensusTimeInterval;
	m_consensusState = qposWaitingVote;
	
	if(1 >= m_miners.size()){
		voteBlockEnd();
		return;
	}
	
	RLPStream msg;
	msg << qposBlockVote;
	msg << m_currentView;
	msg << mySign;
	msg << m_blockBytes; 

	m_voteTimeOut = utcTime() + QPOS_HEART_TIMEOUT;
	multicast(m_miners, msg);
	cdebug << ",header.hash(WithoutSeal)=" << header.hash(WithoutSeal) << ",mySign=" << mySign << ",m_blockBytes.size()=" << m_blockBytes.size() << ",m_miners.size()=" << m_miners.size();
}

void Qpos::generateSealBegin(bytes const& _block)
{
	BlockHeader header(_block);
	if(m_blockNumber+1 != header.number()){
		cdebug << "m_blockNumber=" << m_blockNumber << ",header.number()=" << header.number();
		return;
	}
	
	m_blockBytes = _block;

	cdebug << "m_miners.size()=" << m_miners.size() << ",m_consensusState=" << m_consensusState << ",m_blockBytes.size()=" << m_blockBytes.size();

	voteBlockBegin();
}

void Qpos::generateSeal(bytes const& _block)
{
	m_onSealGenerated(_block, false);
	return;
	
	m_blocks.push(_block);
	kill(m_hostTid, QPOS_SIGNAL);
}

bool Qpos::shouldSeal(Interface * _client)
{
	int64_t blockNumber = _client->number();
	bool isLeader = m_isLeader;
	bool ret = m_consensusState == qposInitial && isLeader;
	int64_t now = utcTime();

	static int64_t s_timeout = 0;
	if(now > s_timeout){
		cdebug << "blockNumber = " << blockNumber << ",m_currentView=" << m_currentView << ",m_consensusState=" << m_consensusState << ",ret=" << ret << ",isLeader=" << isLeader;
		s_timeout = now + 10000;
	}
	return ret;
}

h512s Qpos::getMinerNodeList()
{	
	h512s ret;
	for(auto i : m_miners)
		ret.push_back(i);

	return ret;
}

bool Qpos::checkBlockSign(BlockHeader const& _header, std::vector<std::pair<u256, Signature>> _sign_list) 
{
	Timer t;
	cdebug << "PBFT::checkBlockSign " << _header.number();

	h512s miner_list;
	if (!getMinerList(miner_list, static_cast<int>(_header.number() - 1))) {
		cwarn << "checkBlockSign failed for getMinerList return false, blk=" <<  _header.number() - 1;
		return false;
	}

	cdebug << "checkBlockSign call getAllNodeConnInfo: blk=" << _header.number() - 1 << ", miner_list.size()=" << miner_list.size();

	unsigned singCount = 0;
	for (auto item : _sign_list) {
		unsigned idx = item.first.convert_to<unsigned>();
		if (idx >=  miner_list.size() || !dev::verify(miner_list[idx], item.second, _header.hash(WithoutSeal))) {
			cdebug << "checkBlockSign failed, verify false, blk=" << _header.number() << ",hash=" << _header.hash(WithoutSeal);
			continue;
		}

		singCount++;
		if(singCount >= (miner_list.size()+1)/2)
			return true;
	}

	cdebug << "checkBlockSign success, blk=" << _header.number() << ",hash=" << _header.hash(WithoutSeal) << ",timecost=" << t.elapsed() / 1000 << "ms";
	return false;
}


