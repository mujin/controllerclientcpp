// -*- coding: utf-8 -*-
// Copyright (C) 2012-2023 MUJIN Inc.
#include <mujincontrollerclient/mujindefinitions.h>
#include <mujincontrollerclient/mujinexceptions.h>

namespace mujin {

void SensorSelectionInfo::LoadFromJson(const rapidjson::Value& rSensorSelectionInfo)
{
    mujinjson::LoadJsonValueByKey(rSensorSelectionInfo, "sensorName", sensorName);
    mujinjson::LoadJsonValueByKey(rSensorSelectionInfo, "sensorLinkName", sensorLinkName);
}

void SensorSelectionInfo::SaveToJson(rapidjson::Value& rSensorSelectionInfo, rapidjson::Document::AllocatorType& alloc) const
{
    rSensorSelectionInfo.SetObject();
    mujinjson::SetJsonValueByKey(rSensorSelectionInfo, "sensorName", sensorName, alloc);
    mujinjson::SetJsonValueByKey(rSensorSelectionInfo, "sensorLinkName", sensorLinkName, alloc);
}

void PickPlaceHistoryItem::Reset()
{
    pickPlaceType.clear();
    locationName.clear();
    containerName.clear();
    eventTimeStampUS = 0;
    object_uri.clear();
    objectpose = Transform();
    localaabb = AABB();
    edgeValidationInfos.clear();
    sensorTimeStampUS = 0;
}

static void _LoadAABBFromJsonValue(const rapidjson::Value& rAABB, mujin::AABB& aabb)
{
    BOOST_ASSERT(rAABB.IsObject());
    BOOST_ASSERT(rAABB.HasMember("pos"));
    BOOST_ASSERT(rAABB.HasMember("extents"));
    const rapidjson::Value& rPos = rAABB["pos"];
    BOOST_ASSERT(rPos.IsArray());
    mujinjson::LoadJsonValue(rPos[0], aabb.pos[0]);
    mujinjson::LoadJsonValue(rPos[1], aabb.pos[1]);
    mujinjson::LoadJsonValue(rPos[2], aabb.pos[2]);
    const rapidjson::Value& rExtents = rAABB["extents"];
    BOOST_ASSERT(rExtents.IsArray());
    mujinjson::LoadJsonValue(rExtents[0], aabb.extents[0]);
    mujinjson::LoadJsonValue(rExtents[1], aabb.extents[1]);
    mujinjson::LoadJsonValue(rExtents[2], aabb.extents[2]);
}

static void _LoadEdgeValidationInfosFromJsonValue(const rapidjson::Value& rEdgeValidationInfos, std::vector<EdgeValidationInfo>& edgeValidationInfos)
{
    if( !rEdgeValidationInfos.IsArray() ) {
        return;
    }
    edgeValidationInfos.clear();
    edgeValidationInfos.resize(rEdgeValidationInfos.GetArray().Size());
    size_t iedge = 0;
    for ( const rapidjson::Value& rEdgeValidationInfo : rEdgeValidationInfos.GetArray() ) {
        BOOST_ASSERT(rEdgeValidationInfo.IsObject());
        mujinjson::LoadJsonValueByKey(rEdgeValidationInfo, "edgeType", edgeValidationInfos[iedge].edgeType);
        mujinjson::LoadJsonValueByKey(rEdgeValidationInfo, "result", edgeValidationInfos[iedge].result);
        mujinjson::LoadJsonValueByKey(rEdgeValidationInfo, "measuredLocalLineSegment", edgeValidationInfos[iedge].measuredLocalLineSegment);
        iedge++;
    }
}

void PickPlaceHistoryItem::LoadFromJson(const rapidjson::Value& rItem)
{
    mujinjson::LoadJsonValueByKey(rItem, "pickPlaceType", pickPlaceType);
    mujinjson::LoadJsonValueByKey(rItem, "locationName", locationName);
    mujinjson::LoadJsonValueByKey(rItem, "containerName", containerName);
    mujinjson::LoadJsonValueByKey(rItem, "eventTimeStampUS", eventTimeStampUS);
    mujinjson::LoadJsonValueByKey(rItem, "object_uri", object_uri);

    const rapidjson::Value::ConstMemberIterator itPose = rItem.FindMember("objectpose");
    if( itPose != rItem.MemberEnd() ) {
        const rapidjson::Value& rObjectPose = itPose->value;;
        if( rObjectPose.IsArray() && rObjectPose.Size() == 7 ) {
            mujinjson::LoadJsonValue(rObjectPose[0], objectpose.quaternion[0]);
            mujinjson::LoadJsonValue(rObjectPose[1], objectpose.quaternion[1]);
            mujinjson::LoadJsonValue(rObjectPose[2], objectpose.quaternion[2]);
            mujinjson::LoadJsonValue(rObjectPose[3], objectpose.quaternion[3]);
            mujinjson::LoadJsonValue(rObjectPose[4], objectpose.translate[0]);
            mujinjson::LoadJsonValue(rObjectPose[5], objectpose.translate[1]);
            mujinjson::LoadJsonValue(rObjectPose[6], objectpose.translate[2]);
        }
    }

    const rapidjson::Value::ConstMemberIterator itLocalAABB = rItem.FindMember("localaabb");
    if( itLocalAABB != rItem.MemberEnd() ) {
        const rapidjson::Value& rLocalAABB = itLocalAABB->value;
        _LoadAABBFromJsonValue(rLocalAABB, localaabb);
    }

    const rapidjson::Value::ConstMemberIterator itEdgeValidationInfos = rItem.FindMember("edgeValidationInfos");
    if( itEdgeValidationInfos != rItem.MemberEnd() ) {
        const rapidjson::Value& rEdgeValidationInfos = itEdgeValidationInfos->value;
        _LoadEdgeValidationInfosFromJsonValue(rEdgeValidationInfos, edgeValidationInfos);
    }
    else {
        edgeValidationInfos.clear();
    }

    mujinjson::LoadJsonValueByKey(rItem, "sensorTimeStampUS", sensorTimeStampUS);
}

void PickPlaceHistoryItem::SaveToJson(rapidjson::Value& rItem, rapidjson::Document::AllocatorType& alloc) const
{
    rItem.SetObject();
    mujinjson::SetJsonValueByKey(rItem, "pickPlaceType", pickPlaceType, alloc);
    mujinjson::SetJsonValueByKey(rItem, "locationName", locationName, alloc);
    mujinjson::SetJsonValueByKey(rItem, "containerName", containerName, alloc);
    mujinjson::SetJsonValueByKey(rItem, "eventTimeStampUS", eventTimeStampUS, alloc);
    mujinjson::SetJsonValueByKey(rItem, "object_uri", object_uri, alloc);
    std::array<Real,7> posearray;
    posearray[0] = objectpose.quaternion[0];
    posearray[1] = objectpose.quaternion[1];
    posearray[2] = objectpose.quaternion[2];
    posearray[3] = objectpose.quaternion[3];
    posearray[4] = objectpose.translate[0];
    posearray[5] = objectpose.translate[1];
    posearray[6] = objectpose.translate[2];
    mujinjson::SetJsonValueByKey(rItem, "objectpose", posearray, alloc);

    rapidjson::Value rLocalAABB; rLocalAABB.SetObject();
    mujinjson::SetJsonValueByKey(rLocalAABB, "pos", localaabb.pos, alloc);
    mujinjson::SetJsonValueByKey(rLocalAABB, "extents", localaabb.extents, alloc);
    rItem.AddMember("localaabb", rLocalAABB, alloc);

    rapidjson::Value rEdgeValidationInfos;
    rEdgeValidationInfos.SetArray();
    rEdgeValidationInfos.Reserve(edgeValidationInfos.size(), alloc);
    for (size_t iedge = 0; iedge < edgeValidationInfos.size(); ++iedge) {
        rapidjson::Value rEdgeValidationInfo; rEdgeValidationInfo.SetObject();
        mujinjson::SetJsonValueByKey(rEdgeValidationInfo, "edgeType", edgeValidationInfos[iedge].edgeType, alloc);
        mujinjson::SetJsonValueByKey(rEdgeValidationInfo, "result", edgeValidationInfos[iedge].result, alloc);
        mujinjson::SetJsonValueByKey(rEdgeValidationInfo, "measuredLocalLineSegment", edgeValidationInfos[iedge].measuredLocalLineSegment, alloc);
        rEdgeValidationInfos.PushBack(rEdgeValidationInfo, alloc);
    }
    rItem.AddMember("edgeValidationInfos", rEdgeValidationInfos, alloc);

    mujinjson::SetJsonValueByKey(rItem, "sensorTimeStampUS", sensorTimeStampUS, alloc);
}

const char* GetExecutionVerificationModeString(ExecutionVerificationMode mode)
{
    switch(mode) {
    case EVM_Never: return "never";
    case EVM_LastDetection: return "lastDetection";
    case EVM_PointCloudOnChange: return "pointCloudOnChange";
    case EVM_PointCloudAlways: return "pointCloudAlways";
    case EVM_PointCloudOnChangeWithDuration: return "pointCloudOnChangeWithDuration";
    case EVM_PointCloudOnChangeFirstCycleOnly: return "pointCloudOnChangeFirstCycleOnly";
    case EVM_PointCloudOnChangeAfterGrab: return "pointCloudOnChangeAfterGrab";
    }
    return "(unknown)";
}

ExecutionVerificationMode GetExecutionVerificationModeFromString(const char* pModeStr, ExecutionVerificationMode defaultMode)
{
    if( !pModeStr || pModeStr[0] == 0 ) {
        return defaultMode;
    }
    if( strcmp(pModeStr, "never") == 0 ) {
        return EVM_Never;
    }
    else if( strcmp(pModeStr, "lastDetection") == 0 ) {
        return EVM_LastDetection;
    }
    else if( strcmp(pModeStr, "pointCloudOnChange") == 0 ) {
        return EVM_PointCloudOnChange;
    }
    else if( strcmp(pModeStr, "pointCloudAlways") == 0 ) {
        return EVM_PointCloudAlways;
    }
    else if( strcmp(pModeStr, "pointCloudOnChangeWithDuration") == 0 ) {
        return EVM_PointCloudOnChangeWithDuration;
    }
    else if( strcmp(pModeStr, "pointCloudOnChangeFirstCycleOnly") == 0 ) {
        return EVM_PointCloudOnChangeFirstCycleOnly;
    }
    else if( strcmp(pModeStr, "pointCloudOnChangeAfterGrab") == 0 ) {
        return EVM_PointCloudOnChangeAfterGrab;
    }
    throw mujinclient::MujinException(str(boost::format("Failed to parse '%s' as ExecutionVerificationMode")%pModeStr), mujinclient::MEC_InvalidArguments);
}

MUJINCLIENT_API const char* GetMinViableRegionRegistrationModeString(MinViableRegionRegistrationMode mode)
{
    switch(mode) {
    case MVRRM_None: return "None";
    case MVRRM_Lift: return "Lift";
    case MVRRM_Drag: return "Drag";
    case MVRRM_PerpendicularDrag: return "PerpendicularDrag";
    }
    return "(unknown)";
}

MUJINCLIENT_API MinViableRegionRegistrationMode GetMinViableRegionRegistrationModeFromString(const char* pModeStr, MinViableRegionRegistrationMode defaultMode)
{
    if( pModeStr[0] == 0 ) {
        return defaultMode;
    }
    if( strcmp(pModeStr, "None") == 0 ) {
        return MVRRM_None;
    }
    if( strcmp(pModeStr, "Lift") == 0 ) {
        return MVRRM_Lift;
    }
    if( strcmp(pModeStr, "Drag") == 0 ) {
        return MVRRM_Drag;
    }
    if( strcmp(pModeStr, "PerpendicularDrag") == 0 ) {
        return MVRRM_PerpendicularDrag;
    }
    throw mujinclient::MujinException(str(boost::format("Failed to parse '%s' as ExecutionVerificationMode")%pModeStr), mujinclient::MEC_InvalidArguments);
}

} // end namespace mujin
