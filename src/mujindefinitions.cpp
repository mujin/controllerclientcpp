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
