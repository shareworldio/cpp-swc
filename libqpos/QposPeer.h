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
/** @file QposPeer.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <mutex>
#include <array>
#include <set>
#include <memory>
#include <utility>

#include <libdevcore/RLP.h>
#include <libdevcore/Guards.h>
#include <libdevcore/SHA3.h>

#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Session.h>
#include <libp2p/Capability.h>
#include <libp2p/HostCapability.h>

#include "QposHost.h"
#include "Common.h"
#include "QposSealEngine.h"

namespace dev
{

using namespace p2p;

/*
using namespace p2p::Session;
using namespace p2p::HostCapabilityFace;
using namespace p2p::HostCapability;
using namespace p2p::Capability;
using namespace p2p::CapDesc;
*/

namespace eth
{



class QposSealEngine;
class QposHost;

class QposPeer: public Capability
{
	friend class QposHost;

public:
	//QposPeer(std::shared_ptr<p2p::SessionFace> _s, p2p::HostCapabilityFace* _h, unsigned _i, p2p::CapDesc const& _cap, uint16_t _capID);
	QposPeer(std::shared_ptr<p2p::SessionFace> _s, p2p::HostCapabilityFace* _h, unsigned _i, p2p::CapDesc const& _cap);

	virtual ~QposPeer();
	QposHost* host() const;

	bool sendBytes(unsigned _t, const bytes &_b);
	static std::string name() { return "Qpos"; }
	static u256 version() { return c_protocolVersion; }
	static unsigned messageCount() { return QposMsgCount; }

	RLPStream& prep(RLPStream& _s, unsigned _id, unsigned _args = 0){ return Capability::prep(_s, _id, _args);};
	void sealAndSend(RLPStream& _s){return Capability::sealAndSend(_s);};
	NodeID id();
protected:
	bytes toBytes(RLP const& _r);
private:
	virtual bool interpret(unsigned _id, RLP const&) override;

	QposSealEngine* m_leader = 0;
};

}
}
