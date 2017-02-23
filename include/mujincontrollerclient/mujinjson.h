// -*- coding: utf-8 -*-
// Copyright (C) 2012-2016 MUJIN Inc.
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
/** \file mujinjson.h
    \brief Wrapper for rapidjson.
 */
#ifndef MUJIN_JSON_H
#define MUJIN_JSON_H

#include <boost/shared_ptr.hpp>
#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "mujinexceptions.h"

namespace mujinjson {

/// \brief gets a string of the Value type for debugging purposes 
inline std::string GetJsonTypeName(const rapidjson::Value& v) {
    int type = v.GetType();
    switch (type) {
        case 0:
            return "Null";
        case 1:
            return "False";
        case 2:
            return "True";
        case 3:
            return "Object";
        case 4:
            return "Array";
        case 5:
            return "String";
        case 6:
            return "Number";
        default:
            return "Unknown";
    }
}

inline std::string DumpJson(const rapidjson::Value& value) {
    rapidjson::StringBuffer stringbuffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(stringbuffer);
    value.Accept(writer);
    return std::string(stringbuffer.GetString(), stringbuffer.GetSize());
}

inline void ParseJson(rapidjson::Document&d, const std::string& str) {
    d.Parse(str.c_str());
    if (d.HasParseError()) {
        std::string substr;
        if (str.length()> 200) {
            substr = str.substr(0, 200);
        } else {
            substr = str;
        }
        throw MujinException("Json string is invalid: " + substr, MEC_InvalidArgument);
    }
}

//store a json value to local data structures
//for compatibility with ptree, type conversion is made. will remove them in the future

template<class T> inline void LoadJsonValue(const rapidjson::Value& v, T& t);

template<class T> inline void SaveJsonValue(rapidjson::Value& v, const T& t, rapidjson::Document::AllocatorType& alloc);

inline void LoadJsonValue(const rapidjson::Value& v, std::string& t) {
    if (v.IsString()) {
        t = v.GetString();
    } else if (v.IsInt64()) {
        t = boost::lexical_cast<std::string>(v.GetInt64());
    } else {
        throw MujinException("Cannot convert json type " + GetJsonTypeName(v) + " to String", MEC_InvalidArgument);
    }

}

inline void LoadJsonValue(const rapidjson::Value& v, int& t) {
    if (v.IsInt()) {
        t = v.GetInt();
    } else if (v.IsString()) {
        t = boost::lexical_cast<int>(v.GetString());
    } else {
        throw MujinException("Cannot convert json type " + GetJsonTypeName(v) + " to Int", MEC_InvalidArgument);
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, unsigned int& t) {
    if (v.IsUint()) {
        t = v.GetUint();
    } else if (v.IsString()) {
        t = boost::lexical_cast<unsigned int>(v.GetString());
    } else {
        throw MujinException("Cannot convert json type " + GetJsonTypeName(v) + " to Int", MEC_InvalidArgument);
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, unsigned long long& t) {
    if (v.IsUint64()) {
        t = v.GetUint64();
    } else if (v.IsString()) {
        t = boost::lexical_cast<unsigned long long>(v.GetString());
    } else {
        throw MujinException("Cannot convert json type " + GetJsonTypeName(v) + " to Int64", MEC_InvalidArgument);
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, uint64_t& t) {
    if (v.IsUint64()) {
        t = v.GetUint64();
    } else if (v.IsString()) {
        t = boost::lexical_cast<uint64_t>(v.GetString());
    } else {
        throw MujinException("Cannot convert json type " + GetJsonTypeName(v) + " to Int64", MEC_InvalidArgument);
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, double& t) {
    if (v.IsNumber()) {
        t = v.GetDouble();
    } else if (v.IsString()) {
        t = boost::lexical_cast<double>(v.GetString());
    } else {
        throw MujinException("Cannot convert json type " + GetJsonTypeName(v) + " to Double", MEC_InvalidArgument);
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, bool& t) {
    if (v.IsInt()) t = v.GetInt();
    else if (v.IsBool()) t = v.GetBool();
    else if (v.IsString())  {
        t = boost::lexical_cast<bool>(v.GetString());
    } else {
        throw MujinException("Cannot convert json type " + GetJsonTypeName(v) + " to Bool", MEC_InvalidArgument);
    }
}

template<class T> inline void LoadJsonValue(const rapidjson::Value& v, std::vector<T>& t);

template<class T> inline void LoadJsonValue(const rapidjson::Value& v, boost::shared_ptr<T>& ptr) {
    T t;
    LoadJsonValue(v, t);
    ptr = boost::shared_ptr<T>(new T(t));
}

template<class T, size_t N> inline void LoadJsonValue(const rapidjson::Value& v, T (&p)[N]) {
    if (v.IsArray()) {
        if (v.GetArray().Size() != N) {
            throw MujinException("Json array size doesn't match", MEC_InvalidArgument);
        }
        size_t i = 0;
        for (rapidjson::Value::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
            LoadJsonValue(*it, p[i]);
            i++;
        }
    } else {
        throw MujinException("Cannot convert json type " + GetJsonTypeName(v) + " to Array", MEC_InvalidArgument);
    }
}

template<class T> inline void LoadJsonValue(const rapidjson::Value& v, std::vector<T>& t) {
    if (v.IsArray()) {
        t.clear();
        t.resize(v.GetArray().Size());
        size_t i = 0;
        for (rapidjson::Value::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
            LoadJsonValue(*it, t[i]);
            i++;
        }
    } else {
        throw MujinException("Cannot convert json type " + GetJsonTypeName(v) + " to Array", MEC_InvalidArgument);
    }
}

//Save a data structure to rapidjson::Value format

inline void SaveJsonValue(rapidjson::Value& v, const std::string& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetString(t.c_str(), alloc);
}

inline void SaveJsonValue(rapidjson::Value& v, const char* t, rapidjson::Document::AllocatorType& alloc) {
    v.SetString(t, alloc);
}

inline void SaveJsonValue(rapidjson::Value& v, int t, rapidjson::Document::AllocatorType& alloc) {
    v.SetInt(t);
}

inline void SaveJsonValue(rapidjson::Value& v, unsigned int t, rapidjson::Document::AllocatorType& alloc) {
    v.SetUint(t);
}

inline void SaveJsonValue(rapidjson::Value& v, long long t, rapidjson::Document::AllocatorType& alloc) {
    v.SetInt64(t);
}

inline void SaveJsonValue(rapidjson::Value& v, unsigned long long t, rapidjson::Document::AllocatorType& alloc) {
    v.SetUint64(t);
}

inline void SaveJsonValue(rapidjson::Value& v, uint64_t t, rapidjson::Document::AllocatorType& alloc) {
    v.SetUint64(t);
}

inline void SaveJsonValue(rapidjson::Value& v, bool t, rapidjson::Document::AllocatorType& alloc) {
    v.SetBool(t);
}

inline void SaveJsonValue(rapidjson::Value& v, double t, rapidjson::Document::AllocatorType& alloc) {
    v.SetDouble(t);
}

inline void SaveJsonValue(rapidjson::Value& v, const rapidjson::Value& t, rapidjson::Document::AllocatorType& alloc) {
    v.CopyFrom(t, alloc);
}

template<class T> inline void SaveJsonValue(rapidjson::Value& v, const std::vector<T>& t, rapidjson::Document::AllocatorType& alloc);

/** do not remove: otherwise boost::shared_ptr could be treated as bool
 */
template<class T> inline void SaveJsonValue(rapidjson::Value& v, const boost::shared_ptr<T>& ptr, rapidjson::Document::AllocatorType& alloc) {
    SaveJsonValue(v, *ptr, alloc);
}

template<class T> inline void SaveJsonValue(rapidjson::Value& v, const std::vector<T>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (size_t i = 0; i < t.size(); ++i) {
        rapidjson::Value tmpv;
        SaveJsonValue(tmpv, t[i], alloc);
        v.PushBack(tmpv, alloc);
    }
}

template<> inline void SaveJsonValue(rapidjson::Value& v, const std::vector<double>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (size_t i = 0; i < t.size(); ++i) {
        v.PushBack(t[i], alloc);
    }
}

template<class T> inline void SaveJsonValue(rapidjson::Document& v, const T& t) {
    SaveJsonValue(v, t, v.GetAllocator());
}


template<class T, class U> inline void SetJsonValueByKey(rapidjson::Value& v, const U& key, const T& t, rapidjson::Document::AllocatorType& alloc);

inline void SaveJsonValue(rapidjson::Value& v, const MujinException& e, rapidjson::Document::AllocatorType& alloc) {
    v.SetObject();
    SetJsonValueByKey(v, "type", (int)(e.GetCode()), alloc);
    SetJsonValueByKey(v, "desc", e.GetCodeString(), alloc);
}

//get one json value by key, and store it in local data structures
template<class T> void inline LoadJsonValueByKey(const rapidjson::Value& v, const char* key, T& t) {
    if (v.HasMember(key)) {
        LoadJsonValue(v[key], t);
    }
}
template<class T, class U> inline void LoadJsonValueByKey(const rapidjson::Value& v, const char* key, T& t, const U& d) {
    if (v.HasMember(key)) {
        LoadJsonValue(v[key], t);
    }
    else {
        t = d;
    }
}

//work the same as LoadJsonValueByKey, but the value is returned instead of being passed as reference
template<class T, class U> T GetJsonValueByKey(const rapidjson::Value& v, const char* key, const U& t) {
    if (v.HasMember(key)) {
        T r;
        LoadJsonValue(v[key], r);
        return r;
    }
    else {
        return T(t);
    }
}
template<class T> inline T GetJsonValueByKey(const rapidjson::Value& v, const char* key) {
    T r = T();
    if (v.HasMember(key)) {
        LoadJsonValue(v[key], r);
    }
    return r;
}

template<class T> inline T GetJsonValueByPath(const rapidjson::Value& v, const char* key) {
    T r;
    const rapidjson::Value *child = rapidjson::Pointer(key).Get(v);
    if (child) {
        LoadJsonValue(*child, r);
    }
    return r;
}

template<class T, class U> T GetJsonValueByPath(const rapidjson::Value& v, const char* key, const U& t) {
    const rapidjson::Value *child = rapidjson::Pointer(key).Get(v);
    if (child) {
        T r;
        LoadJsonValue(*child, r);
        return r;
    }
    else {
        return T(t);
    }
}

template<class T, class U> inline void SetJsonValueByKey(rapidjson::Value& v, const U& key, const T& t, rapidjson::Document::AllocatorType& alloc)
{
    if (v.HasMember(key)) {
        SaveJsonValue(v[key], t, alloc);
    }
    else {
        rapidjson::Value value, name;
        SaveJsonValue(name, key, alloc);
        SaveJsonValue(value, t, alloc);
        v.AddMember(name, value, alloc);
    }
}

template<class T>
inline void SetJsonValueByKey(rapidjson::Document& d, const char* key, const T& t)
{
    SetJsonValueByKey(d, key, t, d.GetAllocator());
}

template<class T>
inline void SetJsonValueByKey(rapidjson::Document& d, const std::string& key, const T& t)
{
    SetJsonValueByKey(d, key.c_str(), t, d.GetAllocator());
}

inline void ValidateJsonString(const std::string& str) {
    rapidjson::Document d;
    if (d.Parse(str.c_str()).HasParseError()) {
        throw MujinException("json string " + str + " is invalid.", MEC_InvalidArgument);
    }
}

template<class T> inline std::string GetJsonString(const T& t) {
    rapidjson::Document d;
    SaveJsonValue(d, t);
    return DumpJson(d);
}

} // namespace mujinjson
#endif
