// -*- coding: utf-8 -*-
// Copyright (C) 2012-2026 MUJIN Inc.
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
/** \file mujinmsgpack.h
    \brief Wrapper for msgpack.
 */
#ifndef MUJIN_CONTROLLERCLIENT_MSGPACK_H
#define MUJIN_CONTROLLERCLIENT_MSGPACK_H

#include <iostream>
#include <string>
#include <vector>

#include <boost/assert.hpp>
#include <rapidjson/document.h>

namespace mujinmsgpack {

void DumpMsgPack(const rapidjson::Value& value, std::ostream& os);
void DumpMsgPack(const rapidjson::Value& value, std::vector<char>& output);
void ParseMsgPack(rapidjson::Document& d, const std::string& str);
void ParseMsgPack(rapidjson::Document& d, std::istream& is);
void ParseMsgPack(rapidjson::Document& d, const void* data, size_t size);

} // namespace mujinmsgpack

#endif // MUJIN_CONTROLLERCLIENT_MSGPACK_H
