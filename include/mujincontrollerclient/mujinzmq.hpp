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
        try {
            _socket->send(request);
        } catch (const zmq::error_t& e) {
            if (e.num() == EAGAIN) {
                CLIENTZMQ_LOG_ERROR("failed to send request, zmq::EAGAIN");
                throw e;
            }
            if (e.num() == EFSM) {
                CLIENTZMQ_LOG_INFO("zmq is in bad state, zmq::EFSM");
            }
            CLIENTZMQ_LOG_INFO("re-creating zmq socket and trying again");
            if (!!_socket) {
                _socket->close();
                _socket.reset();
            }
            _InitializeSocket(_context);
            CLIENTZMQ_LOG_INFO("try to send again");
            _socket->send(request);
        }

        //recv
        zmq::message_t reply;

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
                throw std::runtime_error("Timed out receiving response");
            }

        } catch (const zmq::error_t& e) {
            if (e.num() == EAGAIN) {
                CLIENTZMQ_LOG_ERROR("failed to receive reply, zmq::EAGAIN");
                throw std::runtime_error("Failed to receive response, zmq::EAGAIN");
            }
            if (e.num() == EFSM) {
                CLIENTZMQ_LOG_INFO("zmq is in bad state, zmq::EFSM");
            }
            CLIENTZMQ_LOG_INFO("re-creating zmq socket");
            if (!!_socket) {
                _socket->close();
                _socket.reset();
            }
            _InitializeSocket(_context);
            throw std::runtime_error("Got exception receiving response");
        }
        return std::string("{}");
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
    ZmqServer(const unsigned int port)
    {
        _port = port;
    }

    virtual ~ZmqServer()
    {
        _DestroySocket();
    }

    unsigned int Recv(std::string& data)
    {
        _socket->recv(&_reply, ZMQ_NOBLOCK);
        //std::string replystring((char *) reply.data(), (size_t) reply.size());
        data.resize(_reply.size());
        std::copy((uint8_t*)_reply.data(), (uint8_t*)_reply.data()+_reply.size(), data.begin());
        return _reply.size(); //replystring;
    }

    void Send(const std::string& message)
    {
        zmq::message_t request(message.size());
        memcpy((void *) request.data(), message.c_str(), message.size());
        _socket->send(request);
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
        _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_REP));
        std::ostringstream port_stream;
        port_stream << _port;
        _socket->bind (("tcp://*:" + port_stream.str()).c_str());
        std::stringstream ss;
        ss << "binded to tcp://*:" <<  _port;
        CLIENTZMQ_LOG_INFO(ss.str());
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
    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    zmq::message_t _reply;
    bool _sharedcontext;
};

} // namespace mujinzmq
#endif // MUJIN_ZMQ_H
