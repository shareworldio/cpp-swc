#include<stdio.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include <boost/move/utility.hpp>  
#include <boost/log/sources/logger.hpp>  
#include <boost/log/sources/record_ostream.hpp>  
#include <boost/log/sources/global_logger_storage.hpp>  
#include <boost/log/utility/setup/file.hpp>  
#include <boost/log/utility/setup/common_attributes.hpp>

//g++ -std=c++11 log.cpp -pthread /usr/lib/x86_64-linux-gnu/libboost_log.a /usr/lib/x86_64-linux-gnu/libboost_thread.a /usr/lib/x86_64-linux-gnu/libboost_system.a  /usr/lib/x86_64-linux-gnu/libboost_filesystem.a /usr/lib/x86_64-linux-gnu/libboost_log_setup.a

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

using namespace logging::trivial;

int boostLogInit()
{
    logging::add_file_log
    (
		keywords::target = "logs",
        keywords::file_name = "%Y-%m-%d_%H-%M-%S.%N.log",                                        /*< file name pattern >*/
        keywords::rotation_size = 128 * 1024 * 1024,                                   /*< rotate files every 10 MiB... >*/
		//keywords::min_free_space = 10*1024*1024*1024ULL,
		keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0), /*< ...or at midnight >*/
        //keywords::format = "[%TimeStamp%]: %Message%",                                 /*< log record format >*/
        keywords::auto_flush = true
    );

    //logging::core::get()->set_filter(logging::trivial::severity>=logging::trivial::info);
	logging::add_common_attributes();

	return 0;
}

