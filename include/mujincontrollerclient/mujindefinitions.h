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

template<class T, size_t N> inline bool FuzzyEquals(const T (&p)[N], const T (&q)[N], double epsilon=1e-4) {
    for (size_t i = 0; i < N; ++i) {
        if (!FuzzyEquals(p[i], q[i], epsilon)) {
            return false;
        }
    }
    return true;
}

template<class T, size_t N> inline bool FuzzyEquals(const std::array<T,N>& p, const std::array<T,N>& q, double epsilon=1e-4) {
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
    std::array<Real,4> quaternion; ///< quaternion [cos(ang/2), axis*sin(ang/2)]
    std::array<Real,3> translate; ///< translation x,y,z
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
    std::array<Real,3> pos; ///< center of AABB
    std::array<Real,3> extents; ///< half extents of AABB
};

class MUJINCLIENT_API SensorSelectionInfo : public mujinjson::JsonSerializable
{
public:
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
class MUJINCLIENT_API PickPlaceHistoryItem : public mujinjson::JsonSerializable
{
public:
    void Reset();

    void LoadFromJson(const rapidjson::Value& rItem) override;
    void SaveToJson(rapidjson::Value& rItem, rapidjson::Document::AllocatorType& alloc) const override;

    std::string pickPlaceType; ///< the type of action that ocurred can be: "picked", "placed", "touched"
    std::string locationName; ///< the name of the location where picking occurred for "picked", where placing occurred when "placed", and where touching occurred for "touched"
    std::string containerName; ///< the name of the container where picking occurred for "picked", where placing occurred when "placed", and where touching occurred for "touched"
    unsigned long long eventTimeStampUS = 0; ///< time that the event ocurred in us (from Linux epoch). For "picked" this is the chuck time, for "placed this is the unchuck time, for "touched" this is the time when the robot supposedly stopped touching/disturbing the object.
    std::string object_uri; ///< the object uri
    Transform objectpose; ///< 7-values in world, unit is usually mm
    AABB localaabb; ///< AABB of object in object frame.
    unsigned long long sensorTimeStampUS = 0; ///< sensor timestamp in us (from Linux epoch) of when the object was detected in the scene
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

///< \brief the type of execution verification to do
enum ExecutionVerificationMode : uint8_t
{
    EVM_Never = 0, ///< never do run-time verification of source.
    EVM_LastDetection = 1, ///< do verification on the last detected items, sometimes there could be a delay so NOT RECOMMENDED.
    EVM_PointCloudOnChange = 2, ///< Do verification on the real-time point cloud data only when container is known to have changed its contents (ie robot went inside). When robot goes into container and leaves, that counts as a change. (RECOMMENDED)
    EVM_PointCloudAlways = 3, ///< do verification on the real-time point cloud data constantly for every snapshot. This setting is important if things other than robot can change the dest container, but can slow things down and have robot be more susceptible to point cloud noise.
    EVM_PointCloudOnChangeWithDuration = 4, ///< Used for objects like cylinders that can continue to move inside the container for some time before getting into a stable state. Same as pointCloudOnChange, it does verification only when container is known to have changed its contents (ie robot went inside), but it continues to take verification and snapshots until "afterChangeDuration" has expired. For example, if afterChangeDuration is 15s, then will continue capturing the point clouds up to 15s after the robot went inside. When robot goes into container and leaves, that counts as a change.
    EVM_PointCloudOnChangeFirstCycleOnly = 5, ///< Only check the point cloud verification on the first execution cycle. This prevents problems if different unexpected containers coming to robot when cycle first starts.
    EVM_PointCloudOnChangeAfterGrab = 6, ///< For dest containers only. Do verification on the real-time point cloud data only when container is known to have changed and after robot has grabbed the part. When robot goes into dest container and leaves, that counts as a change. Enabling this option means the robot will stop more while grabbing object.
    EVM_PointCloudWaitForTrigger = 7, ///< For eye-in-hand systems. Do verification every time it is triggered through binpicking TriggerDetectionCaptureInfo.
};

MUJINCLIENT_API const char* GetExecutionVerificationModeString(ExecutionVerificationMode mode);

MUJINCLIENT_API ExecutionVerificationMode GetExecutionVerificationModeFromString(const char* pModeStr, ExecutionVerificationMode defaultMode);


enum MinViableRegionRegistrationMode : uint8_t {
    MVRRM_None = 0, ///< registration without touching
    MVRRM_Lift = 1,
    MVRRM_Drag = 2,
    MVRRM_PerpendicularDrag = 3,
};

MUJINCLIENT_API const char* GetMinViableRegionRegistrationModeString(MinViableRegionRegistrationMode mode);

MUJINCLIENT_API MinViableRegionRegistrationMode GetMinViableRegionRegistrationModeFromString(const char* pModeStr, MinViableRegionRegistrationMode defaultMode);

} // end namespace mujin

#endif
