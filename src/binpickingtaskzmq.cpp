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

#include <mujincontrollerclient/binpickingtaskzmq.h>
#include <boost/property_tree/json_parser.hpp>
#include "mujinzmq.h"

namespace mujinclient {

class ZmqMujinControllerClient : public mujinzmq::ZmqClient
{

public:
    ZmqMujinControllerClient(const std::string& host, const int port);

    virtual ~ZmqMujinControllerClient();

    std::string Call(const std::string& msg);
};

ZmqMujinControllerClient::ZmqMujinControllerClient(const std::string& host, const int port) : ZmqClient(host, port)
{
    _InitializeSocket();
}

ZmqMujinControllerClient::~ZmqMujinControllerClient()
{
    // _DestroySocket() is called in  ~ZmqClient()
}

std::string ZmqMujinControllerClient::Call(const std::string& msg)
{
    //send
    //std::cout << "serialization done" << std::endl;
    zmq::message_t request (msg.size());
    memcpy ((void *) request.data (), msg.c_str(), msg.size());
    _socket->send(request);

    //recv
    zmq::message_t reply;

    _socket->recv (&reply);
    std::string replystring((char *) reply.data (), (size_t) reply.size());
    return replystring;
}

BinPickingResultZmqResource::BinPickingResultZmqResource()
{
}

BinPickingResultZmqResource::~BinPickingResultZmqResource()
{
}

void BinPickingResultZmqResource::GetResultPtree(boost::property_tree::ptree& pt) const
{
    boost::property_tree::read_json(result, pt);
}

BinPickingTaskZmqResource::BinPickingTaskZmqResource(const std::string& controllerip, const int zmqport)
{
    _zmqmujincontrollerclient.reset(new ZmqMujinControllerClient(controllerip, zmqport));
    if (!_zmqmujincontrollerclient) {
        throw MujinException(boost::str(boost::format("Failed to establish ZMQ connection to mujin controller at %s:%d")%controllerip%zmqport), MEC_Failed);
    }
}

BinPickingTaskZmqResource::~BinPickingTaskZmqResource()
{
}

void BinPickingTaskZmqResource::UpdateObjects(const std::string& basename, const std::vector<BinPickingTaskParameters::DetectedObject>& detectedobjects)
{
    BinPickingTaskParameters param;
    param.command = "UpdateObjects";
    param.targetname = basename;
    param.detectedobjects = detectedobjects;
    std::string paramstr;
    param.GetJsonString(paramstr);
    _zmqmujincontrollerclient->Call(paramstr);
}

void BinPickingTaskZmqResource::AddPointCloudObstacle(const std::vector<Real>& vpoints, const Real pointsize, const std::string& name)
{
    BinPickingTaskParameters param;
    param.command = "AddPointCloudObstacle";
    BinPickingTaskParameters::PointCloudObstacle pointcloudobstacle;
    pointcloudobstacle.name = name;
    pointcloudobstacle.pointsize = pointsize;
    pointcloudobstacle.points = vpoints;
    param.pointcloudobstacle = pointcloudobstacle;
    std::string paramstr;
    param.GetJsonString(paramstr);
    _zmqmujincontrollerclient->Call(paramstr);
}

void BinPickingTaskZmqResource::IsRobotOccludingBody(const std::string& bodyname, const std::string& sensorname, const unsigned long long starttime, const unsigned long long endtime, bool r)
{
    BinPickingTaskParameters param;
    param.command = "IsRobotOccludingBody";
    BinPickingTaskParameters::SensorOcclusionCheck check;
    check.bodyname = bodyname;
    check.sensorname = sensorname;
    check.starttime = starttime;
    check.endtime = endtime;
    BinPickingResultZmqResource result;
    std::string paramstr;
    param.GetJsonString(paramstr);
    result.result = _zmqmujincontrollerclient->Call(paramstr);
    result.GetResultIsRobotOccludingBody(r);
}

void BinPickingTaskZmqResource::GetPickedPositions(BinPickingResultResource::ResultPickedPositions& r)
{
    BinPickingTaskParameters param;
    param.command = "GetPickedPositions";
    BinPickingResultZmqResource result;
    std::string paramstr;
    param.GetJsonString(paramstr);
    result.result = _zmqmujincontrollerclient->Call(paramstr);
    result.GetResultGetPickedPositions(r);
}

void BinPickingTaskZmqResource::GetTaskParameters(BinPickingTaskParameters& taskparameters) const
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::SetTaskParameters(const BinPickingTaskParameters& taskparameters)
{
    throw MujinException("not implemented yet");
}

PlanningResultResourcePtr BinPickingTaskZmqResource::GetResult()
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::GetJointValues(const int timeout /* [sec] */, BinPickingResultResource::ResultGetJointValues& result)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::MoveJoints(const std::vector<Real>& jointvalues, const std::vector<int>& jointindices, const Real speed /* 0.1-1 */, const int timeout /* [sec] */, BinPickingResultResource::ResultMoveJoints& result)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::GetTransform(const std::string& targetname, Transform& result)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::SetTransform(const std::string& targetname, const Transform& transform)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::GetManipTransformToRobot(Transform& result)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::GetManipTransform(Transform& result)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::GetRobotTransform(Transform& result)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::GetSensorData(const std::string& sensorname, RobotResource::AttachedSensorResource::SensorData& result)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::DeleteObject(const std::string& name)
{
    throw MujinException("not implemented yet");
}

void BinPickingTaskZmqResource::InitializeZMQ(const int zmqport)
{
    throw MujinException("not implemented yet");
}

} // end namespace mujinclient
