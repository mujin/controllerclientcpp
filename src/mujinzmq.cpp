#include "mujincontrollerclient/mujinzmq.h"

#include <boost/thread.hpp>
#if BOOST_VERSION > 104800
#include <boost/algorithm/string/replace.hpp>
#endif
#include <sstream>
#include <iostream>

#include "common.h"
#include "logging.h"

MUJIN_LOGGER("mujin.controllerclientcpp.mujinzmq");


#if defined(_MSC_VER) && _MSC_VER < 1600
typedef __int64             int64_t;
typedef unsigned __int64    uint64_t;
#endif


#ifndef MUJIN_TIME
#define MUJIN_TIME
#include <time.h>

#ifndef _WIN32
#if !(defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0))
#include <sys/time.h>
#endif
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sys/timeb.h>    // ftime(), struct timeb
inline void usleep(unsigned long microseconds) {
    Sleep((microseconds+999)/1000);
}
#endif

#ifdef _WIN32
inline uint64_t GetMilliTime()
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (uint64_t)((count.QuadPart * 1000) / freq.QuadPart);
}

inline uint64_t GetMicroTime()
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (count.QuadPart * 1000000) / freq.QuadPart;
}

inline uint64_t GetNanoTime()
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (count.QuadPart * 1000000000) / freq.QuadPart;
}

inline static uint64_t GetNanoPerformanceTime() {
    return GetNanoTime();
}

#else

inline void GetWallTime(uint32_t& sec, uint32_t& nsec)
{
#if defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0)
    struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);
    sec  = start.tv_sec;
    nsec = start.tv_nsec;
#else
    struct timeval timeofday;
    gettimeofday(&timeofday,NULL);
    sec  = timeofday.tv_sec;
    nsec = timeofday.tv_usec * 1000;
#endif
}

inline uint64_t GetMilliTimeOfDay()
{
    struct timeval timeofday;
    gettimeofday(&timeofday,NULL);
    return (uint64_t)timeofday.tv_sec*1000+(uint64_t)timeofday.tv_usec/1000;
}

inline uint64_t GetNanoTime()
{
    uint32_t sec,nsec;
    GetWallTime(sec,nsec);
    return (uint64_t)sec*1000000000 + (uint64_t)nsec;
}

inline uint64_t GetMicroTime()
{
    uint32_t sec,nsec;
    GetWallTime(sec,nsec);
    return (uint64_t)sec*1000000 + (uint64_t)nsec/1000;
}

inline uint64_t GetMilliTime()
{
    uint32_t sec,nsec;
    GetWallTime(sec,nsec);
    return (uint64_t)sec*1000 + (uint64_t)nsec/1000000;
}

inline static uint64_t GetNanoPerformanceTime()
{
#if defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0) && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec start;
    uint32_t sec, nsec;
    clock_gettime(CLOCK_MONOTONIC, &start);
    sec  = start.tv_sec;
    nsec = start.tv_nsec;
    return (uint64_t)sec*1000000000 + (uint64_t)nsec;
#else
    return GetNanoTime();
#endif
}
#endif
#endif


using namespace mujinzmq;
using namespace mujinclient;

ZmqSubscriber::ZmqSubscriber(const std::string& host, const unsigned int port)
{
    _host = host;
    _port = port;
}

ZmqSubscriber::~ZmqSubscriber()
{
    _DestroySocket();
}

void ZmqSubscriber::_InitializeSocket(boost::shared_ptr<zmq::context_t> context)
{
    if (!!context) {
        _context = context;
        _sharedcontext = true;
    } else {
        _context.reset(new zmq::context_t(1));
        _sharedcontext = false;
    }
    _socket.reset(new zmq::socket_t ((*_context.get()), ZMQ_SUB));
    _socket->set(zmq::sockopt::tcp_keepalive, 1); // turn on tcp keepalive, do these configuration before connect
    _socket->set(zmq::sockopt::tcp_keepalive_idle, 2); // the interval between the last data packet sent (simple ACKs are not considered data) and the first keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
    _socket->set(zmq::sockopt::tcp_keepalive_intvl, 2); // the interval between subsequential keepalive probes, regardless of what the connection has exchanged in the meantime
    _socket->set(zmq::sockopt::tcp_keepalive_cnt, 2); // the number of unacknowledged probes to send before considering the connection dead and notifying the application layer
    _socket->set(zmq::sockopt::sndhwm, 2);
    _socket->set(zmq::sockopt::linger, 100); // ms
    _socket->connect("tcp://" + _host + ':' + std::to_string(_port));
    _socket->set(zmq::sockopt::subscribe, "");
}

void ZmqSubscriber::_DestroySocket()
{
    if (!!_socket) {
        _socket->close();
        _socket.reset();
    }
    if (!!_context && !_sharedcontext) {
        _context->close();
        _context.reset();
    }
}

ZmqPublisher::ZmqPublisher(const unsigned int port)
{
    _port = port;
}

ZmqPublisher::~ZmqPublisher()
{
    _DestroySocket();
}

bool ZmqPublisher::Publish(const std::string& messagestr)
{
    zmq::message_t message(messagestr.size());
    memcpy(message.data(), messagestr.data(), messagestr.size());
    return (bool)_socket->send(message, zmq::send_flags::none);
}

void ZmqPublisher::_InitializeSocket(boost::shared_ptr<zmq::context_t> context)
{
    if (!!context) {
        _context = context;
        _sharedcontext = true;
    }
    else {
        _context.reset(new zmq::context_t(1));
        _sharedcontext = false;
    }
    _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_PUB));
    _socket->set(zmq::sockopt::tcp_keepalive, 1); // turn on tcp keepalive, do these configuration before connect
    _socket->set(zmq::sockopt::tcp_keepalive_idle, 2); // the interval between the last data packet sent (simple ACKs are not considered data) and the first keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
    _socket->set(zmq::sockopt::tcp_keepalive_intvl, 2); // the interval between subsequential keepalive probes, regardless of what the connection has exchanged in the meantime
    _socket->set(zmq::sockopt::tcp_keepalive_cnt, 2); // the number of unacknowledged probes to send before considering the connection dead and notifying the application layer
    _socket->set(zmq::sockopt::sndhwm, 2);
    _socket->set(zmq::sockopt::linger, 100); // ms
    _socket->bind("tcp://*:" + std::to_string(_port));
}

void ZmqPublisher::_DestroySocket()
{
    if (!!_socket) {
        _socket->close();
        _socket.reset();
    }
    if (!!_context && !_sharedcontext) {
        _context->close();
        _context.reset();
    }
}


ZmqClient::ZmqClient(const std::string& host, const unsigned int port, const CheckPreemptFn& preemptfn)
{
    _host = host;
    _port = port;
    _preemptfn = preemptfn;
}

ZmqClient::~ZmqClient()
{
    _DestroySocket();
}

std::string ZmqClient::Call(const std::string& msg, const double timeout, const unsigned int checkpreemptbits)
{
    //send
    zmq::message_t request (msg.size());
    // std::cout << msg.size() << std::endl;
    // std::cout << msg << std::endl;
    memcpy ((void *) request.data (), msg.c_str(), msg.size());

    uint64_t starttime = GetMilliTime();
    bool recreatedonce = false;
    while (GetMilliTime() - starttime < timeout*1000.0) {
        try {
            _socket->send(request, zmq::send_flags::none);
            break;
        } catch (const zmq::error_t& e) {
            if (e.num() == EAGAIN) {
                MUJIN_LOG_ERROR("failed to send request, try again");
                boost::this_thread::sleep(boost::posix_time::milliseconds(100));
                continue;
            } else {
                std::stringstream errss;
                errss << "failed to send msg: ";
                if (msg.length() > 1000) {
                    errss << msg.substr(0, 1000) << "...";
                } else {
                    errss << msg;
                }
                MUJIN_LOG_ERROR(errss.str());
            }
            if (!recreatedonce) {
                MUJIN_LOG_INFO("re-creating zmq socket and trying again");
                if (!!_socket) {
                    _socket->close();
                    _socket.reset();
                }
                _InitializeSocket(_context);
                recreatedonce = true;
            } else{
                std::string ss = "Failed to send request after re-creating socket.";
                MUJIN_LOG_ERROR(ss);
                throw MujinException(ss, MEC_Failed);
            }
        }
        if( !!_preemptfn ) {
            _preemptfn(checkpreemptbits);
        }

    }
    if (GetMilliTime() - starttime > timeout*1000.0) {
        std::string ss = "Timed out trying to send request.";
        MUJIN_LOG_ERROR(ss);
        if (msg.length() > 1000) {
            MUJIN_LOG_INFO(msg.substr(0,1000) << "...");
        } else {
            MUJIN_LOG_INFO(msg);
        }
        throw MujinException(ss, MEC_Timeout);
    }
    //recv
    recreatedonce = false;
    zmq::message_t reply;
    bool receivedonce = false; // receive at least once
    while (!receivedonce || (GetMilliTime() - starttime < timeout * 1000.0)) {
        if( !!_preemptfn ) {
            _preemptfn(checkpreemptbits);
        }

        try {

            zmq::pollitem_t pollitem;
            memset(&pollitem, 0, sizeof(zmq::pollitem_t));
            pollitem.socket = _socket->operator void*();
            pollitem.events = ZMQ_POLLIN;

            // if timeout param is 0, caller means infinite
            long timeoutms = -1;
            if (timeout > 0) {
                timeoutms = timeout * 1000.0;
            }

            zmq::poll(&pollitem, 1, std::chrono::milliseconds{timeoutms});
            receivedonce = true;
            if (pollitem.revents & ZMQ_POLLIN) {
                _socket->recv(reply);
                return reply.to_string();
            } else{
                std::stringstream ss;
                if (msg.length() > 1000) {
                    ss << "Timed out receiving response of command " << msg.substr(0, 1000) << "... after " << timeout << " seconds";
                } else {
                    ss << "Timed out receiving response of command " << msg << " after " << timeout << " seconds";
                }
                MUJIN_LOG_ERROR(ss.str());
#if BOOST_VERSION > 104800
                std::string errstr = ss.str();
                boost::replace_all(errstr, "\"", ""); // need to remove " in the message so that json parser works
                boost::replace_all(errstr, "\\", ""); // need to remove \ in the message so that json parser works
#else
                std::vector< std::pair<std::string, std::string> > serachpairs(2);
                serachpairs[0].first = "\""; serachpairs[0].second = "";
                serachpairs[1].first = "\\"; serachpairs[1].second = "";
                std::string errstr;
                mujinclient::SearchAndReplace(errstr, ss.str(), serachpairs);
#endif
                throw MujinException(errstr, MEC_Timeout);
            }

        } catch (const zmq::error_t& e) {
            if (e.num() == EAGAIN) {
                MUJIN_LOG_ERROR("failed to receive reply, zmq::EAGAIN");
                boost::this_thread::sleep(boost::posix_time::milliseconds(100));
                continue;
            } else {
                MUJIN_LOG_INFO("failed to send");
                if (msg.length() > 1000) {
                    MUJIN_LOG_INFO(msg.substr(0,1000) << "...");
                } else {
                    MUJIN_LOG_INFO(msg);
                }
            }
            if (!recreatedonce) {
                MUJIN_LOG_INFO("re-creating zmq socket and trying again");
                if (!!_socket) {
                    _socket->close();
                    _socket.reset();
                }
                _InitializeSocket(_context);
                recreatedonce = true;
            } else{
                std::string errstr = "Failed to receive response after re-creating socket.";
                MUJIN_LOG_ERROR(errstr);
                throw MujinException(errstr, MEC_Failed);
            }
        }
    }
    if (GetMilliTime() - starttime > timeout*1000.0) {
        std::string ss = "timed out trying to receive request";
        MUJIN_LOG_ERROR(ss);
        if (msg.length() > 1000) {
            MUJIN_LOG_INFO(msg.substr(0,1000) << "...");
        } else {
            MUJIN_LOG_INFO(msg);
        }
        throw MujinException(ss, MEC_Failed);
    }

    return "";
}

void ZmqClient::_InitializeSocket(boost::shared_ptr<zmq::context_t> context)
{
    if (!!context) {
        _context = context;
        _sharedcontext = true;
    } else {
        _context.reset(new zmq::context_t(1));
        _sharedcontext = false;
    }
    _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_REQ));
    _socket->set(zmq::sockopt::tcp_keepalive, 1); // turn on tcp keepalive, do these configuration before connect
    _socket->set(zmq::sockopt::tcp_keepalive_idle, 2); // the interval between the last data packet sent (simple ACKs are not considered data) and the first keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
    _socket->set(zmq::sockopt::tcp_keepalive_intvl, 2); // the interval between subsequential keepalive probes, regardless of what the connection has exchanged in the meantime
    _socket->set(zmq::sockopt::tcp_keepalive_cnt, 2); // the number of unacknowledged probes to send before considering the connection dead and notifying the application layer
    std::string endpoint = "tcp://" + _host + ':' + std::to_string(_port);
    MUJIN_LOG_INFO("connecting to socket at " + endpoint);
    _socket->connect(endpoint);
}

void ZmqClient::_DestroySocket()
{
    if (!!_socket) {
        _socket->set(zmq::sockopt::linger, 0);
        _socket->close();
        _socket.reset();
    }
    if (!!_context && !_sharedcontext) {
        _context->close();
        _context.reset();
    }
}

ZmqServer::ZmqServer(const unsigned int port) : _sharedcontext(false), _port(port) {
}

ZmqServer::~ZmqServer() {
    _DestroySocket();
}

unsigned int ZmqServer::Recv(std::string& data, long timeout)
{
    // wait timeout in millisecond for message
    if (timeout > 0) {
        zmq::poll(&_pollitem, 1, std::chrono::milliseconds{timeout});
        if ((_pollitem.revents & ZMQ_POLLIN) == 0)
        {
            // did not receive anything
            return 0;
        }
    }

    const bool ret = (bool)_socket->recv(_reply, zmq::recv_flags::dontwait);
    if (ret && _reply.size() > 0) {
        data = _reply.to_string();
        return _reply.size();
    } else {
        return 0;
    }
}

void ZmqServer::Send(const std::string& message)
{
    zmq::message_t request(message.size());
    memcpy((void *)request.data(), message.c_str(), message.size());
    _socket->send(request, zmq::send_flags::none);
}

void ZmqServer::_InitializeSocket(boost::shared_ptr<zmq::context_t> context)
{
    if (!!context) {
        _context = context;
        _sharedcontext = true;
    } else {
        _context.reset(new zmq::context_t(1));
        _sharedcontext = false;
    }

    _socket.reset(new zmq::socket_t((*(zmq::context_t*)_context.get()), ZMQ_REP));
    _socket->set(zmq::sockopt::tcp_keepalive, 1); // turn on tcp keepalive, do these configuration before connect
    _socket->set(zmq::sockopt::tcp_keepalive_idle, 2); // the interval between the last data packet sent (simple ACKs are not considered data) and the first keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
    _socket->set(zmq::sockopt::tcp_keepalive_intvl, 2); // the interval between subsequential keepalive probes, regardless of what the connection has exchanged in the meantime
    _socket->set(zmq::sockopt::tcp_keepalive_cnt, 2); // the number of unacknowledged probes to send before considering the connection dead and notifying the application layer

    // setup the pollitem
    memset(&_pollitem, 0, sizeof(_pollitem));
    _pollitem.socket = _socket->operator void*();
    _pollitem.events = ZMQ_POLLIN;

    std::string endpoint = "tcp://*:" + std::to_string(_port);
    _socket->bind(endpoint);
    MUJIN_LOG_INFO("binded to " + endpoint);
}

void ZmqServer::_DestroySocket()
{
    if (!!_socket) {
        _socket->set(zmq::sockopt::linger, 0);
        _socket->close();
        _socket.reset();
    }
    if (!!_context && !_sharedcontext) {
        _context->close();
        _context.reset();
    }
    memset(&_pollitem, 0, sizeof(_pollitem));
}
