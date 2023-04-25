// -*- coding: utf-8 -*-
// Copyright (C) 2012-2023 MUJIN Inc.
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
/** \file mujindefinitions.h

    Includes basic struct, enum, and other definitions that are used throughout the Mujin Controller system.
 */
#ifndef MUJIN_CONTROLLERCLIENT_DEFINITIONS_H
#define MUJIN_CONTROLLERCLIENT_DEFINITIONS_H

#include <mujincontrollerclient/config.h>
#include <mujincontrollerclient/mujinjson.h>

#include <ostream>

namespace mujin {

typedef double Real;

inline bool FuzzyEquals(Real p, Real q, double epsilon=1e-3) {
    return fabs(double(p - q)) < epsilon;
}

template<class T> inline bool FuzzyEquals(const std::vector<T>& p, const std::vector<T>& q, double epsilon=1e-3) {
    if (p.size() != q.size()) {
        return false;
    }
    for (size_t i = 0; i < p.size(); ++i) {
        if (!FuzzyEquals(p[i], q[i], epsilon)) {
            return false;
        }
    }
    return true;
}

template<class T, size_t N> inline bool FuzzyEquals(const T (&p)[N], const T (&q)[N], double epsilon=1e-3) {
    for (size_t i = 0; i < N; ++i) {
        if (!FuzzyEquals(p[i], q[i], epsilon)) {
            return false;
        }
    }
    return true;
}

/// \brief an affine transform
struct Transform
{
    Transform() {
        quaternion[0] = 1; quaternion[1] = 0; quaternion[2] = 0; quaternion[3] = 0;
        translate[0] = 0; translate[1] = 0; translate[2] = 0;
    }
    inline bool operator!=(const Transform& other) const {
        return !FuzzyEquals(quaternion, other.quaternion) || !FuzzyEquals(translate, other.translate);
    }
    inline bool operator==(const Transform& other) const {
        return !operator!=(other);
    }
    Real quaternion[4]; ///< quaternion [cos(ang/2), axis*sin(ang/2)]
    Real translate[3]; ///< translation x,y,z
};

struct AABB
{
    AABB() {
        pos[0] = 0; pos[1] = 0; pos[2] = 0;
        extents[0] = 0; extents[1] = 0; extents[2] = 0;
    }
    inline bool operator!=(const AABB& other) const {
        return !FuzzyEquals(pos, other.pos) || !FuzzyEquals(pos, other.pos);
    }
    inline bool operator==(const AABB& other) const {
        return !operator!=(other);
    }
    Real pos[3]; ///< center of AABB
    Real extents[3]; ///< half extents of AABB
};

struct MUJINCLIENT_API SensorSelectionInfo : public mujinjson::JsonSerializable
{
    SensorSelectionInfo() = default;
    SensorSelectionInfo(const std::string& sensorNameIn, const std::string& sensorLinkNameIn) : sensorName(sensorNameIn), sensorLinkName(sensorLinkNameIn) {
    }
    inline bool operator<(const SensorSelectionInfo& rhs) const {
        if( sensorName == rhs.sensorName ) {
            return sensorLinkName < rhs.sensorLinkName;
        }
        return sensorName < rhs.sensorName;
    }
    inline bool operator==(const SensorSelectionInfo& rhs) const {
        return sensorName == rhs.sensorName && sensorLinkName == rhs.sensorLinkName;
    }
    inline bool operator!=(const SensorSelectionInfo& rhs) const {
        return sensorName != rhs.sensorName || sensorLinkName != rhs.sensorLinkName;
    }

    void LoadFromJson(const rapidjson::Value& rSensorSelectionInfo) override;
    void SaveToJson(rapidjson::Value& rSensorSelectionInfo, rapidjson::Document::AllocatorType& alloc) const override;

    std::string sensorName;
    std::string sensorLinkName;
};


/// \brief the picking history being published from the slave. Anytime the robot goes inside of the source container, its pick history will be udpated.
struct MUJINCLIENT_API PickPlaceHistoryItem
{
    std::string pickPlaceType; ///< the type of action that ocurred can be: "picked", "placed", "touched"
    std::string locationName; ///< the name of the region where picking occurred for "picked", where placing occurred when "placed", and where touching occurred for "touched"
    unsigned long long eventTimeStampUS; ///< time that the event ocurred in us (from Linux epoch). For "picked" this is the chuck time, for "placed this is the unchuck time, for "touched" this is the time when the robot supposedly stopped touching/disturbing the object.
    std::string object_uri; ///< the object uri
    Transform objectpose; ///< 7-values in world, unit is usually mm
    AABB localaabb; ///< AABB of object in object frame.
    unsigned long long sensorTimeStampUS; ///< sensor timestamp in us (from Linux epoch) of when the object was detected in the scene
};

/// \brief Holds the state of each region coming from the planning side.
struct LocationTrackingInfo
{
    std::string locationName; ///< name of the location tracking
    std::string containerId; ///< containerId currently in the location
    std::string containerName; ///< name of the container tracking
    std::string containerUsage; ///< how the container is used
    std::string cycleIndex; ///< unique cycleIndex that is tracking this location
};

struct LocationExecutionInfo
{
    std::string locationName; ///< name of the location tracking
    std::string containerId; ///< containerId currently in the location
    uint64_t forceRequestStampMS=0; ///< ms, (linux epoch) of when the requestion for results (detection or point cloud) came in. If there is no request, then this will be 0
    uint64_t lastInsideContainerStampMS = 0; ///< ms, (linux epoch) of when the robot (or something else) was inside the container and could have potentially disturbed the contents.
    std::string needContainerState; ///< one of: Unknown, NewContainerNeeded, ContainerNeeded, ContainerNotNeeded. If empty, then not initialized yet, so can assume Unknown.
};

inline std::ostream& operator<<(std::ostream& os, const SensorSelectionInfo& rhs)
{
    os << rhs.sensorName << ":" << rhs.sensorLinkName;
    return os;
}

} // end namespace mujin

#endif
