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
/** @file QposPeer.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include <libdevcore/Log.h>
#include <libp2p/All.h>
#include "QposHost.h"
#include "Common.h"

using namespace std;
using namespace dev;
using namespace dev::p2p;
using namespace dev::eth;

QposPeer::QposPeer(std::shared_ptr<p2p::SessionFace> _s, p2p::HostCapabilityFace* _h, unsigned _i, p2p::CapDesc const& _cap): 
	Capability(_s, _h, _i)
{
	(void)_cap;
	QposHost* host = (QposHost*)_h;
	m_leader = host->Leader();
}

QposPeer::~QposPeer()
{
}

QposHost* QposPeer::host() const
{
	return static_cast<QposHost*>(Capability::hostCapability());
}

bytes QposPeer::toBytes(RLP const& _r)
{
	try{
		return _r.toBytes();
	}catch(...){
		return bytes();
	}

	return bytes();
}

bool QposPeer::interpret(unsigned _id, RLP const& _r)
{
	bool ret = m_leader && m_leader->interpret(this, _id, _r);

	return ret;
}

bool QposPeer::sendBytes(unsigned _t, const bytes &_b)
{
	RLPStream s;
	prep(s, _t, 1) << _b;

	sealAndSend(s);
	return true;
}

NodeID QposPeer::id()
{
	std::shared_ptr<SessionFace> s = session();
	if(s)
		return s->id();

	return NodeID();
}


