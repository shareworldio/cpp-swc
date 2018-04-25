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

#include <libethereum/EthereumHost.h>
#include <libp2p/Host.h>
#include "QposClient.h"
#include "Common.h"
#include "Qpos.h"
#include "QposHost.h"
using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace p2p;

QposClient& dev::eth::asQposClient(Interface& _c)
{
	if (dynamic_cast<Qpos*>(_c.sealEngine()))
		return dynamic_cast<QposClient&>(_c);
	throw NotQposSealEngine();
}

QposClient* dev::eth::asQposClient(Interface* _c)
{
	if (dynamic_cast<Qpos*>(_c->sealEngine()))
		return &dynamic_cast<QposClient&>(*_c);
	throw NotQposSealEngine();
}

QposClient::QposClient(
    ChainParams const& _params,
    int _networkID,
    p2p::Host* _host,
    std::shared_ptr<GasPricer> _gpForAdoption,
    boost::filesystem::path const& _dbPath,
    boost::filesystem::path const& _snapshotPath,
    WithExisting _forceAction,
    TransactionQueue::Limits const& _limits
):
	Client(_params, _networkID, _host, _gpForAdoption, _dbPath, _snapshotPath, _forceAction, _limits)
{
	// will throw if we're not an Qpos seal engine.
	asQposClient(*this);

	init(_params, _host);
}

QposClient::~QposClient() {
	raft()->cancelGeneration();
	stopWorking();
}

void QposClient::init(ChainParams const&, p2p::Host *_host) {
	raft()->onSealGenerated([ = ](bytes const & _block, bool _isOurs) {
		if (!this->submitSealed(_block, _isOurs)) {
			clog(ClientNote) << "Submitting block failed...";
		} else {
			//Guard l(x_last_block);
			//m_last_commited_block = BlockHeader(header, HeaderData);
			//u256 time = m_last_commited_block.timestamp();
		}
	});

	raft()->initEnv(this, _host, &m_bc);
	clog(ClientNote) << "Init QposClient success";
}

Qpos* QposClient::raft() const
{
	return dynamic_cast<Qpos*>(Client::sealEngine());
}

void QposClient::startSealing() {
	setName("Client");
	raft()->reportBlock(bc().number(), true);
	raft()->startGeneration();
	Client::startSealing();
}

void QposClient::stopSealing() {
	Client::stopSealing();
	raft()->cancelGeneration();
}

void QposClient::onNewBlocks(h256s const& _blocks, h256Hash& io_changed)
{
	Client::onNewBlocks(_blocks, io_changed);

	unsigned blockNumber = 0;	
	for(auto it : _blocks){
		if(m_bc.number(it) > blockNumber)
			blockNumber = bc().number(it);
	}

	raft()->reportBlock(blockNumber);
}

bool QposClient::submitSealed(bytes const& _block, bool _isOurs)
{/*
	cdebug << "Client_submitSealed: _block.size() = " << _block.size() << ",_isOurs=" << _isOurs;

	try{
		UpgradableGuard l(x_working);
		{
			RLPStream ret;
			cdebug << "submitSealed ret: _block.size() = " << _block.size() << ",_isOurs=" << _isOurs;
			BlockHeader header(_block);
			cdebug << "submitSealed ret: _block.size() = " << _block.size() << ",header.hash()=" << header.hash();
			header.streamRLP(ret);
			bytes head;
			ret.swapOut(head);
			cdebug << "submitSealed ret: head.size() = " << head.size() << ",_isOurs=" << _isOurs;
			if (!m_working.sealBlock(head))
				return false;

			cdebug << "submitSealed ret: _block.size() = " << _block.size() << ",_isOurs=" << _isOurs;
			DEV_WRITE_GUARDED(x_postSeal)
				m_postSeal = m_working;
		}
	}catch(...){
		cdebug << "submitSealed err: _block.size() = " << _block.size() << ",_isOurs=" << _isOurs;
	}
*/
	cdebug << "Client_submitSealed: _block.size() = " << _block.size() << ",_isOurs=" << _isOurs;
	if(_block.empty()){
		m_needStateReset = true;
		return true;
	}
	
	// OPTIMISE: very inefficient to not utilise the existing OverlayDB in m_postSeal that contains all trie changes.
	return m_bq.import(&_block, _isOurs) == ImportResult::Success;
}

bool QposClient::wouldSeal() const
{
	bool empty = false;
	DEV_READ_GUARDED(x_working)
		empty = m_working.empty();

	return !empty && Client::wouldSeal();
}

void QposClient::rejigSealing()
{
	bytes blockBytes;
	if (wouldSeal())
	{
		if (sealEngine()->shouldSeal(this))
		{
			m_wouldButShouldnot = false;

			clog(ClientTrace) << "Rejigging seal engine...";
			DEV_WRITE_GUARDED(x_working)
			{
				if (m_working.isSealed())
				{
					clog(ClientNote) << "Tried to seal sealed block...";
					m_needStateReset = true;
					return;
				}
				

				u256 now_time = u256(utcTime());
				auto last_exec_finish_time = raft()->lastConsensusTime();
				if (!m_noEmptyBlock && now_time - last_exec_finish_time <= 1000) {
					//cdebug << "Wait for next interval last_exec_finish_time:" << last_exec_finish_time << ",now:" << now_time << ", interval=" << now_time - last_exec_finish_time;
					//return;
				}

				m_working.commitToSeal(bc(), m_extraData);
				m_sealingInfo = m_working.info();

				RLPStream h;
				m_sealingInfo.streamRLP(h);

				m_working.sealBlock(h.out());
			}
			DEV_READ_GUARDED(x_working)
			{
				DEV_WRITE_GUARDED(x_postSeal)
					m_postSeal = m_working;
				
				blockBytes = m_working.blockData();
				cdebug << "m_working.info().hash()=" << m_working.info().hash() << ",m_working.isSealed()=" << m_working.isSealed();
			}

			if (wouldSeal())
			{
				ctrace << "Generating seal on" << m_sealingInfo.hash() << "#" << m_sealingInfo.number();
				raft()->generateSeal(blockBytes);
			}
		}
		else
			m_wouldButShouldnot = true;
	}{
		//usleep(1000000);
		//cdebug << "not would seal";
	}
	
	if (!m_wouldSeal)
		sealEngine()->cancelGeneration();

}

