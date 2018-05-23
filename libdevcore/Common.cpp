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
/** @file Common.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Common.h"
#include "Exceptions.h"
#include "Log.h"
#include "BuildInfo.h"

#include <sys/time.h>

using namespace std;
using namespace dev;

namespace dev
{

char const* Version = ETH_PROJECT_VERSION;
bytes const NullBytes;
std::string const EmptyString;

void InvariantChecker::checkInvariants(HasInvariants const* _this, char const* _fn, char const* _file, int _line, bool _pre)
{
    if (!_this->invariants())
    {
        cwarn << (_pre ? "Pre" : "Post") << "invariant failed in" << _fn << "at" << _file << ":" << _line;
        ::boost::exception_detail::throw_exception_(FailedInvariant(), _fn, _file, _line);
    }
}

struct TimerChannel: public LogChannel { static const char* name(); static const int verbosity = 0; };

#if defined(_WIN32)
const char* TimerChannel::name() { return EthRed " ! "; }
#else
const char* TimerChannel::name() { return EthRed " ⚡ "; }
#endif

TimerHelper::~TimerHelper()
{
    auto e = std::chrono::high_resolution_clock::now() - m_t;
    if (!m_ms || e > chrono::milliseconds(m_ms))
        clog(TimerChannel) << m_id << chrono::duration_cast<chrono::milliseconds>(e).count() << "ms";
}

int64_t utcTime()
{
	// TODO: Fix if possible to not use time(0) and merge only after testing in all platforms
	// time_t t = time(0);
	// return mktime(gmtime(&t));
	//return time(0);

	struct timeval tv;    
   	gettimeofday(&tv, NULL);    
   	return tv.tv_sec * TIME_PER_SECOND + (tv.tv_usec * TIME_PER_SECOND) / 1000000;
}

string inUnits(bigint const& _b, strings const& _units)
{
    ostringstream ret;
    u256 b;
    if (_b < 0)
    {
        ret << "-";
        b = (u256)-_b;
    }
    else
        b = (u256)_b;

    u256 biggest = 1;
    for (unsigned i = _units.size() - 1; !!i; --i)
        biggest *= 1000;

    if (b > biggest * 1000)
    {
        ret << (b / biggest) << " " << _units.back();
        return ret.str();
    }
    ret << setprecision(3);

    u256 unit = biggest;
    for (auto it = _units.rbegin(); it != _units.rend(); ++it)
    {
        auto i = *it;
        if (i != _units.front() && b >= unit)
        {
            ret << (double(b / (unit / 1000)) / 1000.0) << " " << i;
            return ret.str();
        }
        else
            unit /= 1000;
    }
    ret << b << " " << _units.front();
    return ret.str();
}

string Demangle(const char* name)
{  
    string dname;
    int status = 0;
    char* pdname = abi::__cxa_demangle(name, NULL, 0, &status);     
    if(status == 0)
    {
        dname.assign(pdname);
        free(pdname);
    }
    else
    {
        dname.assign("[unknown function]");
    }
    return dname;
}

void DumpStack(void)
{
	int nptrs;
	#define SIZE 10000
	void *buffer[SIZE];
	char **strings;
	nptrs = backtrace(buffer, SIZE);
 
	strings = backtrace_symbols(buffer, nptrs);
	if (strings == NULL) {
		cdebug << "backtrace_symbols";
		exit(EXIT_FAILURE);
	}

	for(int i = 1; i < nptrs; ++i)
	{
        const string funinfo(strings[i]);
        const int posleftparenthesis = funinfo.rfind('(');
        const int posplus = funinfo.rfind('+');
        const int posleftbracket = funinfo.rfind('[');
        const int posrightbracket = funinfo.rfind(']');

        long long unsigned int offset = strtoull(funinfo.substr(posleftbracket+1, posrightbracket-posleftbracket-1).c_str(), NULL, 0);
        const string module_name = funinfo.substr(0, posleftparenthesis);
        const string function = Demangle(funinfo.substr(posleftparenthesis+1, posplus-posleftparenthesis-1).c_str());

		cdebug << offset << "#" << module_name << "#" << function;
	}
	
	free(strings);
}

}
