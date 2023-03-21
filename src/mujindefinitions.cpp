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

} // end namespace mujin
