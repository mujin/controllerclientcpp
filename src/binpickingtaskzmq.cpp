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

#include "common.h"
#include "controllerclientimpl.h"
#include "binpickingtaskzmq.h"
#include <algorithm> // find
#include <memory>

#include "logging.h"
#include "mujincontrollerclient/mujinjsonmsgpack.h"
#include "mujincontrollerclient/mujinmasterslaveclient.h"

MUJIN_LOGGER("mujin.controllerclientcpp.binpickingtask.zmq");

using namespace mujinzmq;

namespace mujinclient {
using namespace utils;
using namespace mujinjson;

class ZmqMujinControllerClient : public mujinmasterslaveclient::RequestSocket
{

public:
    ZmqMujinControllerClient(zmq::context_t &context, const std::string& host, const int port): RequestSocket(context, (boost::format("tcp://%s:%d") % host % port).str()) {}
};

BinPickingTaskZmqResource::BinPickingTaskZmqResource(ControllerClientPtr c, const std::string& pk, const std::string& scenepk, const std::string& tasktype) : BinPickingTaskResource(c, pk, scenepk, tasktype)
{
    _callerid = str(boost::format("controllerclientcpp%s_zmq")%MUJINCLIENT_VERSION_STRING);
}

BinPickingTaskZmqResource::~BinPickingTaskZmqResource()
{
}

void BinPickingTaskZmqResource::Initialize(const std::string& defaultTaskParameters,  const int zmqPort, const int heartbeatPort, boost::shared_ptr<zmq::context_t> zmqcontext, const bool initializezmq, const double reinitializetimeout, const double timeout, const std::string& userinfo, const std::string& slaverequestid)
{
    _zmqmujincontrollerclient.reset();
    BinPickingTaskResource::Initialize(defaultTaskParameters, zmqPort, heartbeatPort, zmqcontext, initializezmq, reinitializetimeout, timeout, userinfo, slaverequestid);

    if (initializezmq) {
        InitializeZMQ(reinitializetimeout, timeout);
    }

    _zmqmujincontrollerclient = std::make_unique<ZmqMujinControllerClient>(*_zmqcontext, _mujinControllerIp, _zmqPort);
    if (!_pHeartbeatMonitorThread) {
        _bShutdownHeartbeatMonitor = false;
        if (reinitializetimeout > 0 ) {
            _pHeartbeatMonitorThread.reset(new boost::thread(boost::bind(&BinPickingTaskZmqResource::_HeartbeatMonitorThread, this, reinitializetimeout, timeout)));
        }
    }
}

void _LogTaskParametersAndThrow(const std::string& taskparameters) {
    std::string errstr;
    if (taskparameters.size() > 1000) {
        errstr = taskparameters.substr(0, 1000 - 3) + "...";
    } else {
        errstr = taskparameters;
    }
    throw MujinException(boost::str(boost::format("Timed out receiving response of command with taskparameters=%s")%errstr), MEC_Timeout);
}

void BinPickingTaskZmqResource::ExecuteCommand(const std::string& taskparameters, rapidjson::Document &pt, const double timeout /* [sec] */, const bool getresult)
{
    rapidjson::Document parsing;
    ParseJson(parsing, taskparameters.data(), taskparameters.size());
    return ExecuteCommand(parsing, pt, timeout);
}

void BinPickingTaskZmqResource::ExecuteCommand(rapidjson::Value& rTaskParameters, rapidjson::Document& rOutput, const double timeout)
{
    rapidjson::Document rCommand; rCommand.SetObject();
    mujinjson::SetJsonValueByKey(rCommand, "fnname", _tasktype == "binpicking" ? "binpicking.RunCommand" : "RunCommand");
    mujinjson::SetJsonValueByKey(rCommand, "stamp", GetMilliTime()*1e-3);
    mujinjson::SetJsonValueByKey(rCommand, "callerid", _GetCallerId());

    {
        rapidjson::Value rTaskParams; rTaskParams.SetObject();
        mujinjson::SetJsonValueByKey(rTaskParams, "tasktype", _tasktype, rCommand.GetAllocator());
        rTaskParams.AddMember(rapidjson::Document::StringRefType("taskparameters"), rTaskParameters, rCommand.GetAllocator());

        {
            rapidjson::Value rSceneParams;
            rSceneParams.CopyFrom(_rSceneParams, rCommand.GetAllocator());
            rTaskParams.AddMember(rapidjson::Document::StringRefType("sceneparams"), rSceneParams, rCommand.GetAllocator());
        }
        rCommand.AddMember(rapidjson::Document::StringRefType("taskparams"), rTaskParams, rCommand.GetAllocator());
    }

    {
        rapidjson::Value rUserInfo;
        rUserInfo.CopyFrom(_rUserInfo, rCommand.GetAllocator());
        rCommand.AddMember(rapidjson::Document::StringRefType("userinfo"), rUserInfo, rCommand.GetAllocator());
    }

    rapidjson::Value rServerCommand(rapidjson::kObjectType);
    if (!_slaverequestid.empty()) {
        rServerCommand.AddMember("slaverequestid", rapidjson::Document::StringRefType(_slaverequestid.data(), _slaverequestid.size()), rCommand.GetAllocator());
    }

    if (!_bIsInitialized) {
        throw MujinException("BinPicking task is not initialized, please call Initialzie() first.", MEC_Failed);
    }

    if (_zmqmujincontrollerclient == nullptr) {
        MUJIN_LOG_ERROR("zmqcontrollerclient is not initialized! initialize");
        _zmqmujincontrollerclient = std::make_unique<ZmqMujinControllerClient>(*_zmqcontext, _mujinControllerIp, _zmqPort);
    }

    GenericMsgpackParser parser(rOutput.GetAllocator());
    try {
        if (!mujinmasterslaveclient::SendAndReceive<rapidjson::Value, GenericMsgpackParser>(*_zmqmujincontrollerclient, rServerCommand, rCommand, parser, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(timeout)))) {
            throw MujinException("Cannot deserialize response", MEC_InvalidState);
        }
    }
    catch (const MujinException& e) {
        MUJIN_LOG_ERROR(e.what());
        if (e.GetCode() == MEC_Timeout) {
            _LogTaskParametersAndThrow(mujinjson::DumpJson(rCommand["taskparams"]));
        }
        else {
            throw;
        }
    }
    parser.Extract().Swap(rOutput);
}

void BinPickingTaskZmqResource::InitializeZMQ(const double reinitializetimeout, const double timeout)
{
}

void BinPickingTaskZmqResource::_HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout)
{
    MUJIN_LOG_DEBUG(str(boost::format("starting controller %s monitoring thread on port %d for slaverequestid=%s.")%_mujinControllerIp%_heartbeatPort%_slaverequestid));
    boost::shared_ptr<zmq::socket_t>  socket;
    ResultGetBinpickingState taskstate;
    std::vector<uint8_t> buffer(1024 * 100);
    rapidjson::Document::AllocatorType allocator(buffer.data(), buffer.size());
    while (!_bShutdownHeartbeatMonitor) {
        if (!!socket) {
            socket->close();
            socket.reset();
        }
        socket.reset(new zmq::socket_t((*_zmqcontext.get()),ZMQ_SUB));
        socket->setsockopt(ZMQ_TCP_KEEPALIVE, 1); // turn on tcp keepalive, do these configuration before connect
        socket->setsockopt(ZMQ_TCP_KEEPALIVE_IDLE, 2); // the interval between the last data packet sent (simple ACKs are not considered data) and the first keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
        socket->setsockopt(ZMQ_TCP_KEEPALIVE_INTVL, 2); // the interval between subsequential keepalive probes, regardless of what the connection has exchanged in the meantime
        socket->setsockopt(ZMQ_TCP_KEEPALIVE_CNT, 2); // the number of unacknowledged probes to send before considering the connection dead and notifying the application layer
        std::stringstream ss; ss << std::setprecision(std::numeric_limits<double>::digits10+1);
        ss << _heartbeatPort;
        socket->connect (("tcp://"+ _mujinControllerIp+":"+ss.str()).c_str());
        socket->set(zmq::sockopt::subscribe, "s"+_slaverequestid);

        zmq::pollitem_t pollitem;
        memset(&pollitem, 0, sizeof(zmq::pollitem_t));
        pollitem.socket = socket->operator void*();
        pollitem.events = ZMQ_POLLIN;
        unsigned long long lastheartbeat = GetMilliTime();
        while (!_bShutdownHeartbeatMonitor && (GetMilliTime() - lastheartbeat) / 1000.0f < reinitializetimeout) {
            zmq::poll(&pollitem,1, 50); // wait 50 ms for message
            if (pollitem.revents & ZMQ_POLLIN) {
                zmq::message_t reply;
                socket->recv(reply);
                if (!reply.more()) {
                    MUJIN_LOG_ERROR("unknown protocol");
                    continue;
                }
                socket->recv(reply);
                if (reply.more()) {
                    MUJIN_LOG_ERROR("unknown protocol");
                    continue;
                }

                allocator.Clear();
                GenericMsgpackParser parser(allocator);
                try {
                    if (!msgpack::parse(reply.data<char>(), reply.size(), parser)) {
                        throw std::runtime_error("unable to parse");
                    }
                    const rapidjson::Value pt = parser.Extract();
                    const rapidjson::Value::ConstMemberIterator iterator = pt.FindMember("taskstate");
                    if (iterator != pt.MemberEnd()) {
                        taskstate.Parse(iterator->value);
                    }
                }
                catch (std::exception const &e) {
                    MUJIN_LOG_ERROR("HeartBeat reply is not expected");
                    MUJIN_LOG_ERROR(e.what());
                    continue;
                }
                {
                    boost::mutex::scoped_lock lock(_mutexTaskState);
                    _taskstate = taskstate;
                }
                //BINPICKING_LOG_ERROR(replystring);

                lastheartbeat = GetMilliTime();
            }
        }
        if (!_bShutdownHeartbeatMonitor) {
            std::stringstream sss; sss << std::setprecision(std::numeric_limits<double>::digits10+1);
            sss << (double)((GetMilliTime() - lastheartbeat)/1000.0f) << " seconds passed since last heartbeat signal, re-intializing ZMQ server.";
            MUJIN_LOG_INFO(sss.str());
        }
    }
    MUJIN_LOG_DEBUG(str(boost::format("Stopped controller %s monitoring thread on port %d for slaverequestid=%s.")%_mujinControllerIp%_heartbeatPort%_slaverequestid));
}



} // end namespace mujinclient
