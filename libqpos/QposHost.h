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
/** @file QposHost.h
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
#include <libdevcore/Worker.h>
#include <libdevcore/Guards.h>
#include <libdevcore/SHA3.h>

#include "QposPeer.h"
#include "QposSealEngine.h"

namespace dev
{
namespace eth
{

class QposSealEngine;

class QposHost: public p2p::HostCapability<QposPeer>
//class EthereumHost: public p2p::HostCapability<EthereumPeer>, Worker	
{
	friend class QposPeer;

public:
	QposHost(QposSealEngine* leader);
	virtual ~QposHost();

	void foreachPeer(std::function<bool(std::shared_ptr<QposPeer>)> const&) const;

	QposSealEngine* Leader() {return m_leader;}
protected:
	

private:
	QposSealEngine* m_leader = 0;
};

}
}
