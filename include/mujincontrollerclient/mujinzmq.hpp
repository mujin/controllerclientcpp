// -*- coding: utf-8 -*-
// Copyright (C) 2012-2015 MUJIN Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/** \file mujinzmq.h
    \brief Communication classes based on ZMQ.
 */
#ifndef MUJIN_ZMQ_H
#define MUJIN_ZMQ_H

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <sstream>
#include <iostream>
#include <exception>

#include "mujincontrollerclient/zmq.hpp"

#ifndef USE_LOG4CPP // logging

#define CLIENTZMQ_LOG_INFO(msg) std::cout << msg << std::endl;
#define CLIENTZMQ_LOG_ERROR(msg) std::cerr << msg << std::endl;

#else

#include <log4cpp/Category.hh>
#include <log4cpp/PropertyConfigurator.hh>

LOG4CPP_LOGGER_N(mujincontrollerclientzmqlogger, "mujincontrollerclient.zmq");

#define CLIENTZMQ_LOG_INFO(msg) LOG4CPP_INFO_S(mujincontrollerclientzmqlogger) << msg;
#define CLIENTZMQ_LOG_ERROR(msg) LOG4CPP_ERROR_S(mujincontrollerclientzmqlogger) << msg;

#endif // logging

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

namespace mujinzmq
{

#include "mujincontrollerclient/config.h"

/** \brief Base class for subscriber
 */
class MUJINCLIENT_API ZmqSubscriber
{
public:
    ZmqSubscriber(const std::string& host, const unsigned int port)
    {
        _host = host;
        _port = port;
    }

    virtual ~ZmqSubscriber()
    {
        _DestroySocket();
    }

protected:
    void _InitializeSocket(boost::shared_ptr<zmq::context_t> context)
    {
        if (!!context) {
            _context = context;
            _sharedcontext = true;
        } else {
            _context.reset(new zmq::context_t(1));
            _sharedcontext = false;
        }
        _socket.reset(new zmq::socket_t ((*_context.get()), ZMQ_SUB));

        std::ostringstream port_stream;
        port_stream << _port;
        _socket->setsockopt(ZMQ_SUBSCRIBE, "", 0);
        int val = 2;
        _socket->setsockopt(ZMQ_SNDHWM, &val, sizeof(val));
        _socket->connect (("tcp://" + _host + ":" + port_stream.str()).c_str());
    }

    void _DestroySocket()
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

    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    std::string _host;
    unsigned int _port;
    bool _sharedcontext;
};

/** \brief Base class for publisher
 */
class MUJINCLIENT_API ZmqPublisher
{
public:
    ZmqPublisher(const unsigned int port)
    {
        _port = port;
    }

    virtual ~ZmqPublisher()
    {
        _DestroySocket();
    }

    bool Publish(const std::string& messagestr)
    {
        zmq::message_t message(messagestr.size());
        memcpy(message.data(), messagestr.data(), messagestr.size());
        return _socket->send(message);
    }

protected:
    void _InitializeSocket(boost::shared_ptr<zmq::context_t> context)
    {
        if (!!context) {
            _context = context;
            _sharedcontext = true;
        } else {
            _context.reset(new zmq::context_t(1));
            _sharedcontext = false;
        }
        _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_PUB));
        int val = 2;
        _socket->setsockopt(ZMQ_SNDHWM, &val, sizeof(val));
        std::ostringstream port_stream;
        port_stream << _port;
        _socket->bind (("tcp://*:" + port_stream.str()).c_str());
    }

    void _DestroySocket()
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

    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    unsigned int _port;
    bool _sharedcontext;
};

/** \brief Base class for client
 */
class MUJINCLIENT_API ZmqClient
{
public:
    ZmqClient(const std::string& host, const unsigned int port)
    {
        _host = host;
        _port = port;
    }

    virtual ~ZmqClient()
    {
        _DestroySocket();
    }

    std::string Call(const std::string& msg, const double timeout=5.0/*secs*/)
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
                _socket->send(request);
                break;
            } catch (const zmq::error_t& e) {
                if (e.num() == EAGAIN) {
                    CLIENTZMQ_LOG_ERROR("failed to send request, try again");
                    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
                    continue;
                } else {
                    CLIENTZMQ_LOG_INFO("failed to send");
                    CLIENTZMQ_LOG_INFO(msg);
                }
                if (!recreatedonce) {
                    CLIENTZMQ_LOG_INFO("re-creating zmq socket and trying again");
                    if (!!_socket) {
                        _socket->close();
                        _socket.reset();
                    }
                    _InitializeSocket(_context);
                    recreatedonce = true;
                } else{
                    std::stringstream ss;
                    ss << "failed to send request after re-creating socket";
                    throw std::runtime_error(ss.str());;
                }
            }
        }
        if (GetMilliTime() - starttime > timeout*1000.0) {
            std::stringstream ss;
            ss << "timed out trying to send request";
            CLIENTZMQ_LOG_ERROR(ss.str());
            CLIENTZMQ_LOG_INFO(msg);
            throw std::runtime_error(ss.str());
        }

        //recv
        starttime = GetMilliTime();
        recreatedonce = false;
        zmq::message_t reply;
        while (GetMilliTime() - starttime < timeout * 1000.0) {
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

                zmq::poll(&pollitem, 1, timeoutms);
                if (pollitem.revents & ZMQ_POLLIN) {
                    _socket->recv(&reply);
                    std::string replystring((char *) reply.data (), (size_t) reply.size());
                    return replystring;
                }
                else{
                    std::stringstream ss;
                    if (msg.length() > 1000) {
                        ss << "Timed out receiving response of command " << msg.substr(0, 1000) << "... after " << timeout << " seconds";
                    } else {
                        ss << "Timed out receiving response of command " << msg << " after " << timeout << " seconds";
                    }
                    throw std::runtime_error(ss.str());
                }

            } catch (const zmq::error_t& e) {
                if (e.num() == EAGAIN) {
                    CLIENTZMQ_LOG_ERROR("failed to receive reply, zmq::EAGAIN");
                    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
                    continue;
                } else {
                    CLIENTZMQ_LOG_INFO("failed to send");
                    CLIENTZMQ_LOG_INFO(msg);
                }
                if (!recreatedonce) {
                    CLIENTZMQ_LOG_INFO("re-creating zmq socket and trying again");
                    if (!!_socket) {
                        _socket->close();
                        _socket.reset();
                    }
                    _InitializeSocket(_context);
                    recreatedonce = true;
                } else{
                    std::stringstream ss;
                    ss << "failed to receive response after re-creating socket";
                    throw std::runtime_error(ss.str());;
                }
            }
        }
        if (GetMilliTime() - starttime > timeout*1000.0) {
            std::stringstream ss;
            ss << "timed out trying to receive request";
            CLIENTZMQ_LOG_ERROR(ss.str());
            CLIENTZMQ_LOG_INFO(msg);
            throw std::runtime_error(ss.str());
        }
    }

protected:
    void _InitializeSocket(boost::shared_ptr<zmq::context_t> context)
    {
        if (!!context) {
            _context = context;
            _sharedcontext = true;
        } else {
            _context.reset(new zmq::context_t(1));
            _sharedcontext = false;
        }
        _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_REQ));
        std::ostringstream port_stream;
        port_stream << _port;
        std::stringstream ss;
        ss << "connecting to socket at " << _host << ":" << _port;
        CLIENTZMQ_LOG_INFO(ss.str());
        _socket->connect (("tcp://" + _host + ":" + port_stream.str()).c_str());
    }

    void _DestroySocket()
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

    unsigned int _port;
    std::string _host;
    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    bool _sharedcontext;
};

/** \brief Base class for server
 */
class MUJINCLIENT_API ZmqServer
{
public:
    ZmqServer(const unsigned int port) : _sharedcontext(false), _port(port) {
    }

    virtual ~ZmqServer() {
        _DestroySocket();
    }

    virtual unsigned int Recv(std::string& data, long timeout=0)
    {
        // wait timeout in millisecond for message
        if (timeout > 0) {
            zmq::poll(&_pollitem, 1, timeout); 
            if ((_pollitem.revents & ZMQ_POLLIN) == 0)
            {
                // did not receive anything
                return 0;
            }
        }

        _socket->recv(&_reply, ZMQ_NOBLOCK);
        data.resize(_reply.size());
        std::copy((uint8_t*)_reply.data(), (uint8_t*)_reply.data() + _reply.size(), data.begin());
        return _reply.size();
    }

    virtual void Send(const std::string& message)
    {
        zmq::message_t request(message.size());
        memcpy((void *)request.data(), message.c_str(), message.size());
        _socket->send(request);
    }

protected:

    virtual void _InitializeSocket(boost::shared_ptr<zmq::context_t> context)
    {
        if (!!context) {
            _context = context;
            _sharedcontext = true;
        } else {
            _context.reset(new zmq::context_t(1));
            _sharedcontext = false;
        }

        _socket.reset(new zmq::socket_t((*(zmq::context_t*)_context.get()), ZMQ_REP));

        // setup the pollitem
        memset(&_pollitem, 0, sizeof(_pollitem));
        _pollitem.socket = _socket->operator void*();
        _pollitem.events = ZMQ_POLLIN;

        std::ostringstream endpoint;
        endpoint << "tcp://*:" << _port;
        _socket->bind(endpoint.str().c_str());
        std::stringstream ss;
        ss << "binded to " << endpoint;
        CLIENTZMQ_LOG_INFO(ss.str());
    }

    virtual void _DestroySocket()
    {
        if (!!_socket) {
            _socket->close();
            _socket.reset();
        }
        if (!!_context && !_sharedcontext) {
            _context->close();
            _context.reset();
        }
        memset(&_pollitem, 0, sizeof(_pollitem));
    }

    bool _sharedcontext;
    unsigned int _port;
    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    zmq::message_t _reply;
    zmq::pollitem_t _pollitem;
    
};

} // namespace mujinzmq
#endif // MUJIN_ZMQ_H
