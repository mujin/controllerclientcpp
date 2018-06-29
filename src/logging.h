#pragma once

#if MUJINCLIENT_LOG4CXX

#include <log4cxx/logger.h>

#define MUJIN_LOGGER(name) \
    static ::log4cxx::LoggerPtr logger = ::log4cxx::Logger::getLogger(name);

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#ifdef LOG4CXX_LOCATION
#undef LOG4CXX_LOCATION
#endif
#define LOG4CXX_LOCATION ::log4cxx::spi::LocationInfo(__FILENAME__, __PRETTY_FUNCTION__, __LINE__)

#define MUJIN_LOG_VERBOSE(msg) LOG4CXX_TRACE(logger, msg);
#define MUJIN_LOG_DEBUG(msg) LOG4CXX_DEBUG(logger, msg);
#define MUJIN_LOG_INFO(msg) LOG4CXX_INFO(logger, msg);
#define MUJIN_LOG_ERROR(msg) LOG4CXX_ERROR(logger, msg);

#else

#define MUJIN_LOGGER(name)
#define MUJIN_LOG_VERBOSE(msg) // empty
#define MUJIN_LOG_DEBUG(msg) // empty
#define MUJIN_LOG_INFO(msg) std::cout << msg << std::endl;
#define MUJIN_LOG_ERROR(msg) std::cerr << msg << std::endl;

#endif // logging
