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
#include "mujincontrollerclient/mujinzmq.h"

#include <algorithm> // find

#include "logging.h"
#include "mujincontrollerclient/mujinjson.h"

MUJIN_LOGGER("mujin.controllerclientcpp.binpickingtask.zmq");

using namespace mujinzmq;

namespace mujinclient {
using namespace utils;
using namespace mujinjson;

class ZmqMujinControllerClient : public mujinzmq::ZmqClient
{

public:
    ZmqMujinControllerClient(boost::shared_ptr<zmq::context_t> context, const std::string& host, const int port);

    virtual ~ZmqMujinControllerClient();

};

ZmqMujinControllerClient::ZmqMujinControllerClient(boost::shared_ptr<zmq::context_t> context, const std::string& host, const int port) : ZmqClient(host, port)
{
    _InitializeSocket(context);
}

ZmqMujinControllerClient::~ZmqMujinControllerClient()
{
    // _DestroySocket() is called in  ~ZmqClient()
}

BinPickingTaskZmqResource::BinPickingTaskZmqResource(ControllerClientPtr c, const std::string& pk, const std::string& scenepk, const std::string& tasktype) : BinPickingTaskResource(c, pk, scenepk, tasktype)
{
    _callerid = str(boost::format("controllerclientcpp%s_zmq")%MUJINCLIENT_VERSION_STRING);
}

BinPickingTaskZmqResource::~BinPickingTaskZmqResource()
{
}

void BinPickingTaskZmqResource::Initialize(const std::string& defaultTaskParameters,  const int zmqPort, const int heartbeatPort, boost::shared_ptr<zmq::context_t> zmqcontext, const bool initializezmq, const double reinitializetimeout, const double timeout, const std::string& userinfo, const std::string& slaverequestid)
{
    BinPickingTaskResource::Initialize(defaultTaskParameters, zmqPort, heartbeatPort, zmqcontext, initializezmq, reinitializetimeout, timeout, userinfo, slaverequestid);

    if (initializezmq) {
        InitializeZMQ(reinitializetimeout, timeout);
    }

    _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
    if (!_zmqmujincontrollerclient) {
        throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%_mujinControllerIp%_zmqPort), MEC_Failed);
    }
    if (!_pHeartbeatMonitorThread) {
        _bShutdownHeartbeatMonitor = false;
        if (reinitializetimeout > 0 ) {
            _pHeartbeatMonitorThread.reset(new boost::thread(boost::bind(&BinPickingTaskZmqResource::_HeartbeatMonitorThread, this, reinitializetimeout, timeout)));
        }
    }
}

void _LogTaskParametersAndThrow(const std::string& taskparameters) {
    std::string errstr;
    if (taskparameters.size()>1000) {
        errstr = taskparameters.substr(0, 1000);
    } else {
        errstr = taskparameters;
    }
    throw MujinException(boost::str(boost::format("Timed out receiving response of command with taskparameters=%s...")%errstr));
}

void BinPickingTaskZmqResource::ExecuteCommand(const std::string& taskparameters, rapidjson::Document &pt, const double timeout /* [sec] */, const bool getresult)
{
    std::stringstream ss; ss << std::setprecision(std::numeric_limits<double>::digits10+1);
    ss << "{\"fnname\": \"";
    ss << (_tasktype == "binpicking" ? "binpicking.RunCommand\", " : "RunCommand\", ");

    ss << "\"stamp\": " << (GetMilliTime()*1e-3) << ", ";
    ss << "\"callerid\": \"" << _GetCallerId() << "\", ";
    ss << "\"taskparams\": {\"tasktype\": \"" << _tasktype << "\", ";

    ss << "\"taskparameters\": " << taskparameters << ", ";
    ss << "\"sceneparams\": " << _sceneparams_json << "}, ";
    ss << "\"userinfo\": " << _userinfo_json;
    if (_slaverequestid != "") {
        ss << ", " << GetJsonString("slaverequestid", _slaverequestid);
    }
    ss << "}";
    std::string result_ss;

    try{
        _ExecuteCommandZMQ(ss.str(), pt, timeout, getresult);
    }
    catch (const MujinException& e) {
        MUJIN_LOG_ERROR(e.what());
        if (e.GetCode() == MEC_Timeout) {
            _LogTaskParametersAndThrow(taskparameters);
        }
        else {
            throw;
        }
    }
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

    if (!_slaverequestid.empty()) {
        mujinjson::SetJsonValueByKey(rCommand, "slaverequestid", _slaverequestid);
    }

    try {
        _ExecuteCommandZMQ(mujinjson::DumpJson(rCommand), rOutput, timeout);
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
}

void BinPickingTaskZmqResource::_ExecuteCommandZMQ(const std::string& command, rapidjson::Document& rOutput, const double timeout, const bool getresult)
{
    if (!_bIsInitialized) {
        throw MujinException("BinPicking task is not initialized, please call Initialzie() first.", MEC_Failed);
    }

    if (!_zmqmujincontrollerclient) {
        MUJIN_LOG_ERROR("zmqcontrollerclient is not initialized! initialize");
        _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
    }

    std::string result_ss;
    try {
        result_ss = _zmqmujincontrollerclient->Call(command, timeout);
    }
    catch (const MujinException& e) {
        MUJIN_LOG_ERROR(e.what());
        if (e.GetCode() == MEC_ZMQNoResponse) {
            MUJIN_LOG_INFO("reinitializing zmq connection with the slave");
            _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(_zmqcontext, _mujinControllerIp, _zmqPort));
            if (!_zmqmujincontrollerclient) {
                throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%_mujinControllerIp%_zmqPort), MEC_Failed);
            }
        } else if (e.GetCode() == MEC_Timeout) {
            throw MujinException("");  // Filled by `ExecuteCommand` callers who can access taskparameters more easily
        }
        else {
            throw;
        }
    }

    try {
        ParseJson(rOutput, result_ss);
    }
    catch(const std::exception& ex) {
        MUJIN_LOG_ERROR(str(boost::format("Could not parse result %s")%result_ss));
        throw;
    }
    if( rOutput.IsObject() && rOutput.HasMember("error")) {
        std::string error = GetJsonValueByKey<std::string>(rOutput["error"], "errorcode");
        std::string description = GetJsonValueByKey<std::string>(rOutput["error"], "description");
        if ( error.size() > 0 ) {
            std::string serror;
            if ( description.size() > 0 ) {
                serror = description;
            }
            else {
                serror = error;
            }
            if( serror.size() > 1000 ) {
                MUJIN_LOG_ERROR(str(boost::format("truncated original error message from %d")%serror.size()));
                serror = serror.substr(0,1000);
            }
            throw MujinException(str(boost::format("Error when calling binpicking.RunCommand: %s")%serror), MEC_BinPickingError);
        }
    }
}

void BinPickingTaskZmqResource::InitializeZMQ(const double reinitializetimeout, const double timeout)
{
}

void BinPickingTaskZmqResource::_HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout)
{
    MUJIN_LOG_DEBUG(str(boost::format("starting controller %s monitoring thread on port %d for slaverequestid=%s.")%_mujinControllerIp%_heartbeatPort%_slaverequestid));
    boost::shared_ptr<zmq::socket_t>  socket;
    BinPickingTaskResource::ResultHeartBeat heartbeat;
    heartbeat._slaverequestid = _slaverequestid;
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
        socket->setsockopt(ZMQ_SUBSCRIBE, "", 0);

        zmq::pollitem_t pollitem;
        memset(&pollitem, 0, sizeof(zmq::pollitem_t));
        pollitem.socket = socket->operator void*();
        pollitem.events = ZMQ_POLLIN;
        unsigned long long lastheartbeat = GetMilliTime();
        while (!_bShutdownHeartbeatMonitor && (GetMilliTime() - lastheartbeat) / 1000.0f < reinitializetimeout) {
            zmq::poll(&pollitem,1, 50); // wait 50 ms for message
            if (pollitem.revents & ZMQ_POLLIN) {
                zmq::message_t reply;
                socket->recv(&reply);
                std::string replystring((char *)reply.data (), (size_t)reply.size());
                rapidjson::Document pt(rapidjson::kObjectType);
                try{
                    std::stringstream replystring_ss(replystring);
                    ParseJson(pt, replystring_ss.str());
                    heartbeat.Parse(pt);
                    {
                        boost::mutex::scoped_lock lock(_mutexTaskState);
                        _taskstate = heartbeat.taskstate;
                    }
                    //BINPICKING_LOG_ERROR(replystring);

                    if (heartbeat.status != std::string("lost") && heartbeat.status.size() > 1) {
                        lastheartbeat = GetMilliTime();
                    }
                }
                catch (std::exception const &e) {
                    MUJIN_LOG_ERROR("HeartBeat reply is not JSON");
                    MUJIN_LOG_ERROR(replystring);
                    MUJIN_LOG_ERROR(e.what());
                    continue;
                }
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
