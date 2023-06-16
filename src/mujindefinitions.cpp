// -*- coding: utf-8 -*-
// Copyright (C) 2012-2023 MUJIN Inc.
#include <mujincontrollerclient/mujindefinitions.h>

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
    return defaultMode;
}

} // end namespace mujin
