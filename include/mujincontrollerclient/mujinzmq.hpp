// -*- coding: utf-8 -*-
// Copyright (C) 2012-2014 MUJIN Inc. <rosen.diankov@mujin.co.jp>
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

#include "mujincontrollerclient/zmq.hpp"

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
    void _InitializeSocket()
    {
        _context.reset(new zmq::context_t (1));
        _socket.reset(new zmq::socket_t ((*_context.get()), ZMQ_SUB));

        std::ostringstream port_stream;
        port_stream << _port;
        _socket->connect (("tcp://" + _host + ":" + port_stream.str()).c_str());
        _socket->setsockopt(ZMQ_SUBSCRIBE, "", 0);
    int val = 1;
    _socket->setsockopt(ZMQ_CONFLATE,&val,sizeof(val));
    }

    void _DestroySocket()
    {
        if (!!_socket) {
            _socket->close();
            _socket.reset();
        }
        if( !!_context ) {
            _context->close();
            _context.reset();
        }
    }

    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    std::string _host;
    unsigned int _port;
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
    void _InitializeSocket()
    {
        _context.reset(new zmq::context_t (1));
        _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_PUB));
        int val = 1;
        _socket->setsockopt(ZMQ_CONFLATE,&val,sizeof(val));
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
        if( !!_context ) {
            _context->close();
            _context.reset();
        }
    }

    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    unsigned int _port;
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

protected:
    void _InitializeSocket()
    {
        _context.reset(new zmq::context_t (1));
        _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_REQ));
        std::ostringstream port_stream;
        port_stream << _port;
        std::cout << "connecting to socket at " << _host << ":" << _port << std::endl;
        _socket->connect (("tcp://" + _host + ":" + port_stream.str()).c_str());
    }

    void _DestroySocket()
    {
        if (!!_socket) {
            _socket->close();
            _socket.reset();
        }
        if( !!_context ) {
            _context->close();
            _context.reset();
        }
    }

    unsigned int _port;
    std::string _host;
    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
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
    void _InitializeSocket()
    {
        _context.reset(new zmq::context_t (1));
        _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_REP));
        std::ostringstream port_stream;
        port_stream << _port;
        _socket->bind (("tcp://*:" + port_stream.str()).c_str());
        std::cout << "binded to tcp://*:" << _port << std::endl;
    }

    void _DestroySocket()
    {
        if (!!_socket) {
            _socket->close();
            _socket.reset();
        }
        if( !!_context ) {
            _context->close();
            _context.reset();
        }
    }
    unsigned int _port;
    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    zmq::message_t _reply;
};

} // namespace mujinvision
#endif // MUJIN_ZMQ_H
