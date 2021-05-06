// -*- coding: utf-8 -*-
// Copyright (C) 2012-2016 MUJIN Inc.
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
#include <boost/function.hpp>

#include <mujincontrollerclient/zmq.hpp>
#include <mujincontrollerclient/mujinexceptions.h>

namespace mujinzmq
{

#include "mujincontrollerclient/config.h"

/** \brief Base class for subscriber
 */
class MUJINCLIENT_API ZmqSubscriber
{
public:
    ZmqSubscriber(const std::string& host, const unsigned int port);
    virtual ~ZmqSubscriber();

protected:
    void _InitializeSocket(boost::shared_ptr<zmq::context_t> context);
    void _DestroySocket();

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
    ZmqPublisher(const unsigned int port);
    virtual ~ZmqPublisher();

    bool Publish(const std::string& messagestr);

    inline boost::shared_ptr<zmq::socket_t> GetSocket() {
        return _socket;
    }

protected:
    void _InitializeSocket(boost::shared_ptr<zmq::context_t> context);
    void _DestroySocket();

    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    unsigned int _port;
    bool _sharedcontext;
};

typedef boost::function<void (const unsigned int)> CheckPreemptFn;

/** \brief Base class for client
 */
class MUJINCLIENT_API ZmqClient
{
public:
    ZmqClient(const std::string& host, const unsigned int port, const CheckPreemptFn& preemptfn=CheckPreemptFn());
    virtual ~ZmqClient();

    /** \brief makes request and receives reply. throws MujinException and UserInterruptException
        \param msg content of the request
        \param timeout in seconds
        \param checkpreemptbits bit number preemptfn would check to interrupt the call
     */
    std::string Call(const std::string& msg, const double timeout=5.0/*secs*/, const unsigned int checkpreemptbits=0);

protected:
    void _InitializeSocket(boost::shared_ptr<zmq::context_t> context);
    void _DestroySocket();

    unsigned int _port;
    std::string _host;
    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    bool _sharedcontext;
    CheckPreemptFn _preemptfn;
};

/** \brief Base class for server
 */
class MUJINCLIENT_API ZmqServer
{
public:
    ZmqServer(const unsigned int port);
    virtual ~ZmqServer();

    virtual unsigned int Recv(std::string& data, long timeout=0);
    virtual void Send(const std::string& message);

protected:
    virtual void _InitializeSocket(boost::shared_ptr<zmq::context_t> context);
    virtual void _DestroySocket();

    bool _sharedcontext;
    unsigned int _port;
    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    zmq::message_t _reply;
    zmq::pollitem_t _pollitem;
    
};

} // namespace mujinzmq
#endif // MUJIN_ZMQ_H
