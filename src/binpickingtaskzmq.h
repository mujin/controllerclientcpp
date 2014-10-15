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
/** \file mujincontrollerclient.h
    \brief  Defines the public headers of the MUJIN Controller Client
 */
#ifndef MUJIN_CONTROLLERCLIENT_BINPICKINGTASK_ZMQ_H
#define MUJIN_CONTROLLERCLIENT_BINPICKINGTASK_ZMQ_H

#include <mujincontrollerclient/binpickingtask.h>
#include "mujinzmq.h"
namespace mujinclient {

/** \brief client to mujin controller via zmq socket connection
 */
class ZmqMujinControllerClient;
typedef boost::shared_ptr<ZmqMujinControllerClient> ZmqMujinControllerClientPtr;
typedef boost::weak_ptr<ZmqMujinControllerClient> ZmqMujinControllerClientWeakPtr;

class MUJINCLIENT_API BinPickingTaskZmqResource : public BinPickingTaskResource
{
public:
    BinPickingTaskZmqResource(ControllerClientPtr controller, const std::string& pk, const std::string& scenepk);

    ~BinPickingTaskZmqResource();

    boost::property_tree::ptree ExecuteCommand(const std::string& command, const double timeout /* [sec] */=0.0, const bool getresult=true);

    void Initialize(const std::string& robotControllerUri, const int zmqPort, const int heartbeatPort, const bool initializezmq=false, const double reinitializetimeout=10, const double timeout=0);

    void InitializeZMQ(const double reinitializetimeout = 5, const double timeout /* second */=0);
    void _HeartbeatMonitorThread(const double reinitializetimeout, const double commandtimeout);
private:
    boost::mutex _zmqmutex;
    ZmqMujinControllerClientPtr _zmqmujincontrollerclient;
};

} // namespace mujinclient
#endif
