
#pragma once

#include <libdevcore/Common.h>
#include <mutex>
#include <condition_variable>
#include <boost/thread.hpp>

#define cqpos LOG(TRACE)

enum QposMsgType
{
	QposMsgQpos = 0x01,

	QposMsgCount
};

enum QposPacketType
{
	qposBlockVote = 0x01,
	qposBlockVoteAck,
	qposVote,
	qposVoteAck,
	qposHeart,
	qposBroadBlock,
	QposTestPacket,

	qposPacketCount
};

struct AutoLock
{
	AutoLock(boost::recursive_mutex & _lock): m_lock(_lock)
	{
		//m_lock.lock();
	}
	
	~AutoLock()
	{
		//m_lock.unlock();
	}

	boost::recursive_mutex & m_lock;
};

#define AUTO_LOCK(x) AutoLock autoLock(x)


