// -*- coding: utf-8 -*-
// Copyright (C) 2012-2017 MUJIN Inc.
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
#ifndef MUJIN_CONTROLLERCLIENT_JSON_H
#define MUJIN_CONTROLLERCLIENT_JSON_H

#include <array>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/assert.hpp>
#include <stdint.h>
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <map>
#include <deque>
#include <iostream>

#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>

#include <mujincontrollerclient/config.h>

#ifndef MUJINJSON_LOAD_REQUIRED_JSON_VALUE_BY_KEY
#define MUJINJSON_LOAD_REQUIRED_JSON_VALUE_BY_KEY(rValue, key, param) \
    { \
        if (!(mujinjson::LoadJsonValueByKey(rValue, key, param))) \
        { \
            throw mujinjson::MujinJSONException(boost::str(boost::format(("[%s, %u] assert(mujinjson::LoadJsonValueByKey(%s, %s, %s))"))%__FILE__%__LINE__%# rValue%key%# param)); \
        } \
    }
#endif // MUJINJSON_LOAD_REQUIRED_JSON_VALUE_BY_KEY

namespace mujinjson {

#ifndef MujinJSONException

enum MujinJSONErrorCode
{
    MJE_Failed=0,
};

// define a MujinJSONException class
inline const char* GetErrorCodeString(MujinJSONErrorCode error)
{
    switch(error) {
    case MJE_Failed: return "Failed";
    }
    // throw an exception?
    return "";
}

/// \brief Exception that MujinJSON internal methods throw; the error codes are held in \ref MujinJSONErrorCode.
class MujinJSONException : public std::exception
{
public:
    MujinJSONException() : std::exception(), _s(""), _error(MJE_Failed) {
    }
    MujinJSONException(const std::string& s, MujinJSONErrorCode error=MJE_Failed ) : std::exception() {
        _error = error;
        _s = "mujin (";
        _s += "): ";
        _s += s;
        _ssub = s;
    }
    virtual ~MujinJSONException() throw() {
    }

    /// \brief outputs everything
    char const* what() const throw() {
        return _s.c_str();
    }

    /// \brief outputs everything
    const std::string& message() const {
        return _s;
    }

    /// \briefs just the sub-message
    const std::string& GetSubMessage() const {
        return _ssub;
    }

    MujinJSONErrorCode GetCode() const {
        return _error;
    }

private:
    std::string _s, _ssub;
    MujinJSONErrorCode _error;
};

#endif

template<class T> inline std::string GetJsonString(const T& t);

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

template<class T> inline std::string GetJsonString(const T& t);

inline std::string DumpJson(const rapidjson::Value& value, const unsigned int indent=0) {
    rapidjson::StringBuffer stringbuffer;
    if (indent == 0) {
        rapidjson::Writer<rapidjson::StringBuffer> writer(stringbuffer);
        value.Accept(writer);
    } else {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(stringbuffer);
        writer.SetIndent(' ', indent);
        value.Accept(writer);
    }
    return std::string(stringbuffer.GetString(), stringbuffer.GetSize());
}

inline void DumpJson(const rapidjson::Value& value, std::ostream& os, const unsigned int indent=0) {
    rapidjson::OStreamWrapper osw(os);
    if (indent == 0) {
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
        value.Accept(writer);
    } else {
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        writer.SetIndent(' ', indent);
        value.Accept(writer);
    }
}

/// \brief this clears the allcoator!
inline void ParseJson(rapidjson::Document& d, const char* str, size_t length)
{
    d.SetNull();
    d.GetAllocator().Clear(); // dangerous if used by other existing objects
    d.Parse<rapidjson::kParseFullPrecisionFlag>(str, length); // parse float in full precision mode
    if (d.HasParseError()) {
        const std::string substr(str, length < 200 ? length : 200);

        throw MujinJSONException(boost::str(boost::format("Json string is invalid (offset %u) %s data is '%s'.")%((unsigned)d.GetErrorOffset())%GetParseError_En(d.GetParseError())%substr));
    }
}

/// \brief this clears the allcoator!
inline void ParseJson(rapidjson::Document& d, const std::string& str) {
    ParseJson(d, str.c_str(), str.size());
}

/// \brief this clears the allcoator!
inline void ParseJson(rapidjson::Document& d, std::istream& is) {
    rapidjson::IStreamWrapper isw(is);
    // see note in: void ParseJson(rapidjson::Document& d, const std::string& str)
    d.SetNull();
    d.GetAllocator().Clear();
    d.ParseStream<rapidjson::kParseFullPrecisionFlag>(isw); // parse float in full precision mode
    if (d.HasParseError()) {
        throw MujinJSONException(boost::str(boost::format("Json stream is invalid (offset %u) %s")%((unsigned)d.GetErrorOffset())%GetParseError_En(d.GetParseError())));
    }
}

inline void ParseJson(rapidjson::Value& r, rapidjson::Document::AllocatorType& alloc, std::istream& is)
{
    rapidjson::IStreamWrapper isw(is);
    rapidjson::GenericReader<rapidjson::Value::EncodingType, rapidjson::Value::EncodingType, rapidjson::Document::AllocatorType> reader(&alloc);
    //ClearStackOnExit scope(*this);

    size_t kDefaultStackCapacity = 1024;
    rapidjson::Document rTemp(&alloc, kDefaultStackCapacity); // needed by Parse to be a document
    rTemp.ParseStream<rapidjson::kParseFullPrecisionFlag>(isw); // parse float in full precision mode
    if (rTemp.HasParseError()) {
        throw MujinJSONException(boost::str(boost::format("Json stream is invalid (offset %u) %s")%((unsigned)rTemp.GetErrorOffset())%GetParseError_En(rTemp.GetParseError())));
    }
    r.Swap(rTemp);

//    rapidjson::ParseResult parseResult = reader.template Parse<rapidjson::kParseFullPrecisionFlag>(isw, rTemp);
//    if( parseResult.IsError() ) {
//        *stack_.template Pop<ValueType>(1)
//        throw MujinJSONException(boost::str(boost::format("Json stream is invalid (offset %u) %s")%((unsigned)parseResult.Offset())%GetParseError_En(parseResult.Code())));
//    }
}

template <typename Container>
MUJINCLIENT_API void ParseJsonFile(rapidjson::Document& d, const char* filename, Container& buffer);

inline void ParseJsonFile(rapidjson::Document& d, const char* filename)
{
    std::vector<char> buffer;

    return ParseJsonFile(d, filename, buffer);
}

template <class Container>
void ParseJsonFile(rapidjson::Document& d, const std::string& filename, Container& buffer)
{
    return ParseJsonFile(d, filename.c_str(), buffer);
}

inline void ParseJsonFile(rapidjson::Document& d, const std::string& filename)
{
    std::vector<char> buffer;

    return ParseJsonFile(d, filename.c_str(), buffer);
}


class JsonSerializable {
public:
    virtual void LoadFromJson(const rapidjson::Value& v) = 0;
    virtual void SaveToJson(rapidjson::Value& v, rapidjson::Document::AllocatorType& alloc) const = 0;
    virtual void SaveToJson(rapidjson::Document& d) const {
        SaveToJson(d, d.GetAllocator());
    }
};

template<class T, class S> inline T LexicalCast(const S& v, const std::string& typeName) {
    try {
        T t = boost::lexical_cast<T>(v);
        return t;
    }
    catch (const boost::bad_lexical_cast& ex) {
        std::stringstream ss;
        ss << v;
        throw MujinJSONException("Cannot convert \"" + ss.str() + "\" to type " + typeName);
    }
}

//store a json value to local data structures
//for compatibility with ptree, type conversion is made. will remove them in the future
inline void LoadJsonValue(const rapidjson::Value& v, JsonSerializable& t) {
    t.LoadFromJson(v);
}

inline void LoadJsonValue(const rapidjson::Value& v, std::string& t) {
    if (v.IsString()) {
        t.assign(v.GetString(), v.GetStringLength()); // specify size to allow null characters
    } else if (v.IsInt64()) {
        //TODO: add warnings on all usages of lexical_cast
        t = LexicalCast<std::string>(v.GetInt64(), "String");
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to String");
    }

}

inline void LoadJsonValue(const rapidjson::Value& v, int& t) {
    if (v.IsInt()) {
        t = v.GetInt();
    } else if (v.IsUint()) {
        t = v.GetUint();
    } else if (v.IsString()) {
        t = LexicalCast<int>(v.GetString(), "Int");
    } else if (v.IsBool()) {
        t = v.GetBool() ? 1 : 0;
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to Int");
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, int8_t& t) {
    if (v.IsInt()) {
        t = v.GetInt();
    }
    else if (v.IsUint()) {
        t = v.GetUint();
    }
    else if (v.IsString()) {
        t = LexicalCast<int>(v.GetString(), "Int8");
    }
    else if (v.IsBool()) {
        t = v.GetBool() ? 1 : 0;
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to Int8");
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, uint8_t& t) {
    if (v.IsUint()) {
        t = v.GetUint();
    } else if (v.IsString()) {
        t = LexicalCast<unsigned int>(v.GetString(), "UInt8");
    } else if (v.IsBool()) {
        t = v.GetBool() ? 1 : 0;
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to UInt8");
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, uint16_t& t) {
    if (v.IsUint()) {
        t = v.GetUint();
    } else if (v.IsString()) {
        t = LexicalCast<uint16_t>(v.GetString(), "UInt");
    } else if (v.IsBool()) {
        t = v.GetBool() ? 1 : 0;
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to UInt");
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, unsigned int& t) {
    if (v.IsUint()) {
        t = v.GetUint();
    } else if (v.IsString()) {
        t = LexicalCast<unsigned int>(v.GetString(), "UInt");
    } else if (v.IsBool()) {
        t = v.GetBool() ? 1 : 0;
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to UInt");
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, unsigned long long& t) {
    if (v.IsUint64()) {
        t = v.GetUint64();
    } else if (v.IsString()) {
        t = LexicalCast<unsigned long long>(v.GetString(), "ULongLong");
    } else if (v.IsBool()) {
        t = v.GetBool() ? 1 : 0;
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to ULongLong");
    }
}

#ifndef _MSC_VER
inline void LoadJsonValue(const rapidjson::Value& v, uint64_t& t) {
    if (v.IsUint64()) {
        t = v.GetUint64();
    } else if (v.IsString()) {
        t = LexicalCast<uint64_t>(v.GetString(), "UInt64");
    } else if (v.IsBool()) {
        t = v.GetBool() ? 1 : 0;
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to UInt64");
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, int64_t& t) {
    if (v.IsInt64()) {
        t = v.GetInt64();
    } else if (v.IsString()) {
        t = LexicalCast<int64_t>(v.GetString(), "Int64");
    } else if (v.IsBool()) {
        t = v.GetBool() ? 1 : 0;
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to Int64");
    }
}
#endif

inline void LoadJsonValue(const rapidjson::Value& v, double& t) {
    if (v.IsNumber()) {
        t = v.GetDouble();
    } else if (v.IsString()) {
        t = LexicalCast<double>(v.GetString(), "Double");
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to Double");
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, float& t) {
    if (v.IsNumber()) {
        t = v.GetDouble();
    } else if (v.IsString()) {
        t = LexicalCast<double>(v.GetString(), "Float");
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to Float");
    }
}

inline void LoadJsonValue(const rapidjson::Value& v, bool& t) {
    if (v.IsInt()) t = v.GetInt();
    else if (v.IsBool()) t = v.GetBool();
    else if (v.IsString())  {
        t = LexicalCast<bool>(v.GetString(), "Bool");
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to Bool");
    }
}

template<class T, class AllocT = std::allocator<T> > inline void LoadJsonValue(const rapidjson::Value& v, std::vector<T, AllocT>& t);

template<class T, size_t N> inline void LoadJsonValue(const rapidjson::Value& v, std::array<T, N>& t);

template<class T> inline void LoadJsonValue(const rapidjson::Value& v, boost::shared_ptr<T>& ptr) {
    static_assert(std::is_default_constructible<T>::value, "Shared pointer of type must be default-constructible.");
    ptr = boost::make_shared<T>();
    LoadJsonValue(v, *ptr);
}

template<typename T, typename U> inline void LoadJsonValue(const rapidjson::Value& v, std::pair<T, U>& t) {
    if (v.IsArray()) {
        if (v.GetArray().Size() == 2) {
            LoadJsonValue(v[0], t.first);
            LoadJsonValue(v[1], t.second);
        } else {
            throw MujinJSONException("List-based map has entry with size != 2");
        }
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to Pair");
    }
}

template<class T, size_t N> inline void LoadJsonValue(const rapidjson::Value& v, T (&p)[N]) {
    if (v.IsArray()) {
        if (v.GetArray().Size() != N) {
            throw MujinJSONException("Json array size doesn't match");
        }
        size_t i = 0;
        for (rapidjson::Value::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
            LoadJsonValue(*it, p[i]);
            i++;
        }
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to Array");
    }
}

template<class T, class AllocT> inline void LoadJsonValue(const rapidjson::Value& v, std::vector<T, AllocT>& t) {
    if (v.IsArray()) {
        t.clear();
        t.resize(v.GetArray().Size());
        size_t i = 0;
        for (rapidjson::Value::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
            LoadJsonValue(*it, t[i]);
            i++;
        }
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to Vector");
    }
}

template<class T, size_t N> inline void LoadJsonValue(const rapidjson::Value& v, std::array<T, N>& t) {
    if (v.IsArray()) {
        if (v.GetArray().Size() != N) {
            throw MujinJSONException(
                      (boost::format("Cannot convert json type " + GetJsonTypeName(v) + " to Array. "
                                                                                        "Array length does not match (%d != %d)") % N % v.GetArray().Size()).str());
        }
        size_t i = 0;
        for (rapidjson::Value::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
            LoadJsonValue(*it, t[i]);
            i++;
        }
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to Array");
    }
}

template<class U> inline void LoadJsonValue(const rapidjson::Value& v, std::map<std::string, U>& t) {
    if (v.IsArray()) {
        // list based map
        // TODO: is it dangerous?
        for (rapidjson::Value::ConstValueIterator itr = v.Begin(); itr != v.End(); ++itr) {
            std::pair<std::string, U> value;
            LoadJsonValue((*itr), value);
            t[value.first] = value.second;
        }
    } else if (v.IsObject()) {
        t.clear();
        for (rapidjson::Value::ConstMemberIterator it = v.MemberBegin();
             it != v.MemberEnd(); ++it) {
            // Deserialize directly into the map to avoid copying temporaries.
            // Note that our key needs to be explicitly length-constructed since
            // it may contain \0 bytes.
            LoadJsonValue(it->value,
                          t[std::string(it->name.GetString(),
                                        it->name.GetStringLength())]);
        }
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to Map");
    }
}

template<class U> inline void LoadJsonValue(const rapidjson::Value& v, std::deque<U>& t) {
    // It doesn't make sense to construct a deque from anything other than a JSON array
    if (!v.IsArray()) {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to deque");
    }

    // Ensure our output is a blank slate
    t.clear();

    // Preallocate to fit the incoming data. Deque has no reserve, only resize.
    t.resize(v.Size());

    // Iterate each array entry and attempt to deserialize it directly as a member type
    typename std::deque<U>::size_type emplaceIndex = 0;
    for (rapidjson::Value::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
        // Deserialize directly into the map to avoid copying temporaries.
        LoadJsonValue(*it, t[emplaceIndex++]);
    }
}

template<class U> inline void LoadJsonValue(const rapidjson::Value& v, std::unordered_map<std::string, U>& t) {
    // It doesn't make sense to construct an unordered map from anything other
    // than a full JSON object
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to unordered_map");
    }

    // Ensure our output is a blank slate
    t.clear();
    for (rapidjson::Value::ConstMemberIterator it = v.MemberBegin();
         it != v.MemberEnd(); ++it) {
        // Deserialize directly into the map to avoid copying temporaries.
        // Note that our key needs to be explicitly length-constructed since it
        // may contain \0 bytes.
        LoadJsonValue(
            it->value,
            t[std::string(it->name.GetString(), it->name.GetStringLength())]);
    }
}

//Save a data structure to rapidjson::Value format

/*template<class T> inline void SaveJsonValue(rapidjson::Value& v, const T& t, rapidjson::Document::AllocatorType& alloc) {*/
/*JsonWrapper<T>::SaveToJson(v, t, alloc);*/
/*}*/

inline void SaveJsonValue(rapidjson::Value& v, const JsonSerializable& t, rapidjson::Document::AllocatorType& alloc) {
    t.SaveToJson(v, alloc);
}

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

#ifndef _MSC_VER
inline void SaveJsonValue(rapidjson::Value& v, int64_t t, rapidjson::Document::AllocatorType& alloc) {
    v.SetInt64(t);
}
#endif

inline void SaveJsonValue(rapidjson::Value& v, unsigned long long t, rapidjson::Document::AllocatorType& alloc) {
    v.SetUint64(t);
}

#ifndef _MSC_VER
inline void SaveJsonValue(rapidjson::Value& v, uint64_t t, rapidjson::Document::AllocatorType& alloc) {
    v.SetUint64(t);
}
#endif

inline void SaveJsonValue(rapidjson::Value& v, bool t, rapidjson::Document::AllocatorType& alloc) {
    v.SetBool(t);
}

inline void SaveJsonValue(rapidjson::Value& v, int8_t t, rapidjson::Document::AllocatorType& alloc) {
    v.SetInt(t);
}

inline void SaveJsonValue(rapidjson::Value& v, double t, rapidjson::Document::AllocatorType& alloc) {
    v.SetDouble(t);
}

inline void SaveJsonValue(rapidjson::Value& v, float t, rapidjson::Document::AllocatorType& alloc) {
    v.SetDouble(t);
}

inline void SaveJsonValue(rapidjson::Value& v, const rapidjson::Value& t, rapidjson::Document::AllocatorType& alloc) {
    v.CopyFrom(t, alloc);
}

template<class T, class AllocT = std::allocator<T> > inline void SaveJsonValue(rapidjson::Value& v, const std::vector<T, AllocT>& t, rapidjson::Document::AllocatorType& alloc);

template<class T, size_t N> inline void SaveJsonValue(rapidjson::Value& v, const std::array<T, N>& t, rapidjson::Document::AllocatorType& alloc);

/** do not remove: otherwise boost::shared_ptr could be treated as bool
 */
template<class T> inline void SaveJsonValue(rapidjson::Value& v, const boost::shared_ptr<T>& ptr, rapidjson::Document::AllocatorType& alloc) {
    SaveJsonValue(v, *ptr, alloc);
}

template<class T, class U> inline void SaveJsonValue(rapidjson::Value& v, const std::pair<T, U>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(2, alloc);
    rapidjson::Value rFirst;
    SaveJsonValue(rFirst, t.first, alloc);
    v.PushBack(rFirst, alloc);
    rapidjson::Value rSecond;
    SaveJsonValue(rSecond, t.second, alloc);
    v.PushBack(rSecond, alloc);
}

template<class T, class AllocT> inline void SaveJsonValue(rapidjson::Value& v, const std::vector<T, AllocT>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (size_t i = 0; i < t.size(); ++i) {
        rapidjson::Value tmpv;
        SaveJsonValue(tmpv, t[i], alloc);
        v.PushBack(tmpv, alloc);
    }
}

template<class T, size_t N> inline void SaveJsonValue(rapidjson::Value& v, const std::array<T, N>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(N, alloc);
    for (size_t i = 0; i < N; ++i) {
        rapidjson::Value tmpv;
        SaveJsonValue(tmpv, t[i], alloc);
        v.PushBack(tmpv, alloc);
    }
}

template<class AllocT = std::allocator<double> > inline void SaveJsonValue(rapidjson::Value& v, const std::vector<double, AllocT>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (size_t i = 0; i < t.size(); ++i) {
        v.PushBack(t[i], alloc);
    }
}

template<class T, size_t N> inline void SaveJsonValue(rapidjson::Value& v, const T (&t)[N], rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(N, alloc);
    for (size_t i = 0; i < N; ++i) {
        rapidjson::Value tmpv;
        SaveJsonValue(tmpv, t[i], alloc);
        v.PushBack(tmpv, alloc);
    }
}

template<size_t N> inline void SaveJsonValue(rapidjson::Value& v, const double (&t)[N], rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(N, alloc);
    for (size_t i = 0; i < N; ++i) {
        v.PushBack(t[i], alloc);
    }
}

template<size_t N> inline void SaveJsonValue(rapidjson::Value& v, const std::array<double, N>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(N, alloc);
    for (size_t i = 0; i < N; ++i) {
        v.PushBack(t[i], alloc);
    }
}

template<class U> inline void SaveJsonValue(rapidjson::Value& v, const std::map<std::string, U>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetObject();
    for (typename std::map<std::string, U>::const_iterator it = t.begin(); it != t.end(); ++it) {
        rapidjson::Value name, value;
        SaveJsonValue(name, it->first, alloc);
        SaveJsonValue(value, it->second, alloc);
        v.AddMember(name, value, alloc);
    }
}

template<class U> inline void SaveJsonValue(rapidjson::Value& v, const std::unordered_map<std::string, U>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetObject();
    for (typename std::unordered_map<std::string, U>::const_iterator it = t.begin(); it != t.end(); ++it) {
        rapidjson::Value name, value;
        SaveJsonValue(name, it->first, alloc);
        SaveJsonValue(value, it->second, alloc);
        v.AddMember(name, value, alloc);
    }
}

template <class T, class AllocT> inline void SaveJsonValue(rapidjson::Value& v, const std::deque<T, AllocT>& t, rapidjson::Document::AllocatorType& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (typename std::deque<T, AllocT>::const_iterator it = t.begin(); it != t.end(); ++it) {
        rapidjson::Value value;
        SaveJsonValue(value, *it, alloc);
        v.PushBack(value, alloc);
    }
}

template<class T> inline void SaveJsonValue(rapidjson::Document& v, const T& t) {
    // rapidjson::Value::CopyFrom also doesn't free up memory, need to clear memory
    // see note in: void ParseJson(rapidjson::Document& d, const std::string& str)
    v.SetNull();
    v.GetAllocator().Clear();
    SaveJsonValue(v, t, v.GetAllocator());
}

template<class T, class U> inline void SetJsonValueByKey(rapidjson::Value& v, const U& key, const T& t, rapidjson::Document::AllocatorType& alloc);

//get one json value by key, and store it in local data structures
//return true if key is present. Will return false if the key is not present or the member is Null
template<class T> bool inline LoadJsonValueByKey(const rapidjson::Value& v, const char* key, T& t) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot load value of non-object.");
    }
    rapidjson::Value::ConstMemberIterator itMember = v.FindMember(key);
    if( itMember != v.MemberEnd() ) {
        const rapidjson::Value& rMember = itMember->value;
        if( !rMember.IsNull() ) {
            try {
                LoadJsonValue(rMember, t);
                return true;
            }
            catch (const MujinJSONException& ex) {
                throw MujinJSONException("Got \"" + ex.message() + "\" while parsing the value of \"" + key + "\"");
            }
        }
    }
    // Return false if member is null or non-existent
    return false;
}
template<class T, class U> inline void LoadJsonValueByKey(const rapidjson::Value& v, const char* key, T& t, const U& d) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot load value of non-object.");
    }

    rapidjson::Value::ConstMemberIterator itMember = v.FindMember(key);
    if( itMember != v.MemberEnd() && !itMember->value.IsNull() ) {
        try {
            LoadJsonValue(itMember->value, t);
        }
        catch (const MujinJSONException& ex) {
            throw MujinJSONException("Got \"" + ex.message() + "\" while parsing the value of \"" + key + "\"");
        }
    }
    else {
        t = d;
    }
}

template<class T> inline void LoadJsonValueByPath(const rapidjson::Value& v, const char* key, T& t) {
    const rapidjson::Value *child = rapidjson::Pointer(key).Get(v);
    if (child && !child->IsNull()) {
        LoadJsonValue(*child, t);
    }
}
template<class T, class U> inline void LoadJsonValueByPath(const rapidjson::Value& v, const char* key, T& t, const U& d) {
    const rapidjson::Value *child = rapidjson::Pointer(key).Get(v);
    if (child && !child->IsNull()) {
        LoadJsonValue(*child, t);
    }
    else {
        t = d;
    }
}


//work the same as LoadJsonValueByKey, but the value is returned instead of being passed as reference
template<class T, class U> T GetJsonValueByKey(const rapidjson::Value& v, const char* key, const U& t) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot get value of non-object.");
    }

    rapidjson::Value::ConstMemberIterator itMember = v.FindMember(key);
    if (itMember != v.MemberEnd() ) {
        const rapidjson::Value& child = itMember->value;
        if (!child.IsNull()) {
            T r;
            try {
                LoadJsonValue(v[key], r);
            }
            catch (const MujinJSONException& ex) {
                throw MujinJSONException("Got \"" + ex.message() + "\" while parsing the value of \"" + key + "\"");
            }
            return r;
        }
    }
    return T(t);
}
template<class T> inline T GetJsonValueByKey(const rapidjson::Value& v, const char* key) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot load value of non-object.");
    }
    T r = T();
    rapidjson::Value::ConstMemberIterator itMember = v.FindMember(key);
    if (itMember != v.MemberEnd() ) {
        const rapidjson::Value& child = itMember->value;
        if (!child.IsNull()) {
            try {
                LoadJsonValue(v[key], r);
            }
            catch (const MujinJSONException& ex) {
                throw MujinJSONException("Got \"" + ex.message() + "\" while parsing the value of \"" + key + "\"");
            }
        }
    }
    return r;
}

inline std::string GetStringJsonValueByKey(const rapidjson::Value& v, const char* key, const std::string& defaultValue=std::string()) {
    return GetJsonValueByKey<std::string, std::string>(v, key, defaultValue);
}

/// \brief default value is returned when there is no key or value is null
inline const char* GetCStringJsonValueByKey(const rapidjson::Value& v, const char* key, const char* pDefaultValue=nullptr) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot load value of non-object.");
    }
    rapidjson::Value::ConstMemberIterator itMember = v.FindMember(key);
    if (itMember != v.MemberEnd() ) {
        const rapidjson::Value& child = itMember->value;
        if (!child.IsNull()) {
            if( child.IsString() ) {
                return child.GetString();
            }
            else {
                throw MujinJSONException("In GetCStringJsonValueByKey, expecting a String, but got a different object type");
            }
        }
    }
    return pDefaultValue; // not present
}

template<class T> inline T GetJsonValueByPath(const rapidjson::Value& v, const char* key) {
    T r;
    const rapidjson::Value *child = rapidjson::Pointer(key).Get(v);
    if (child && !child->IsNull()) {
        LoadJsonValue(*child, r);
    }
    return r;
}

#if __cplusplus >= 201103
template<class T, class U=T>
#else
template<class T, class U>
#endif
T GetJsonValueByPath(const rapidjson::Value& v, const char* key, const U& t) {
    const rapidjson::Value *child = rapidjson::Pointer(key).Get(v);
    if (child && !child->IsNull()) {
        T r;
        LoadJsonValue(*child, r);
        return r;
    }
    else {
        return T(t);
    }
}

inline const rapidjson::Value* GetJsonPointerByPath(const rapidjson::Value& value)
{
    return &value;
}

template <size_t N, typename ... Ps> const rapidjson::Value* GetJsonPointerByPath(const rapidjson::Value& value, const char (& key)[N], Ps&&... ps)
{
    const rapidjson::Value::ConstMemberIterator it = value.FindMember(rapidjson::Value(key, N - 1));
    if (it != value.MemberEnd()) {
        return GetJsonPointerByPath(it->value, static_cast<Ps&&>(ps)...);
    } else {
        return nullptr;
    }
}

template<class T> inline void SetJsonValueByPath(rapidjson::Value& d, const char* path, const T& t, rapidjson::Document::AllocatorType& alloc) {
    rapidjson::Value v;
    SaveJsonValue(v, t, alloc);
    rapidjson::Pointer(path).Swap(d, v, alloc);
}

template<class T> inline void SetJsonValueByPath(rapidjson::Document& d, const char* path, const T& t) {
    SetJsonValueByPath(d, path, t, d.GetAllocator());
}

template<class T, class U> inline void SetJsonValueByKey(rapidjson::Value& v, const U& key, const T& t, rapidjson::Document::AllocatorType& alloc)
{
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot set value for non-object.");
    }
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
        throw MujinJSONException("json string " + str + " is invalid." + GetParseError_En(d.GetParseError()));
    }
}

template<class T> inline std::string GetJsonString(const T& t) {
    rapidjson::Document d;
    SaveJsonValue(d, t);
    return DumpJson(d);
}

template<class T, class U> inline std::string GetJsonStringByKey(const U& key, const T& t) {
    rapidjson::Document d(rapidjson::kObjectType);
    SetJsonValueByKey(d, key, t);
    return DumpJson(d);
}

/** update a json object with another one, new key-value pair will be added, existing ones will be overwritten
 */
inline void UpdateJson(rapidjson::Document& a, const rapidjson::Value& b) {
    if (!a.IsObject()) {
        throw MujinJSONException("json object should be a dict to be updated: " + GetJsonString(a));
    }
    if (!b.IsObject()) {
        throw MujinJSONException("json object should be a dict to update another dict: " + GetJsonString(b));
    }
    for (rapidjson::Value::ConstMemberIterator it = b.MemberBegin(); it != b.MemberEnd(); ++it) {
        SetJsonValueByKey(a, it->name.GetString(), it->value);
    }
}

/// \brief One way merge overrideValue into sourceValue. sourceValue will be overwritten. When both sourceValue and overrideValue are objects and a member of overrideValue is a rapidjson object and the corresponding member of sourceValue is also a rapidjson object, call this function recursivelly inside this function.
/// \param sourceValue json value to be updated
/// \param overrideValue read-only json value used to update contents of sourceValue
/// \return true if sourceValue has changed
inline bool UpdateJsonRecursively(rapidjson::Value& sourceValue, const rapidjson::Value& overrideValue, rapidjson::Document::AllocatorType& alloc) {
    if (sourceValue == overrideValue) {
        return false;
    }

    if (!sourceValue.IsObject() || !overrideValue.IsObject()) {
        sourceValue.CopyFrom(overrideValue, alloc, true);
        return true;
    }

    bool hasChanged = false;
    for (rapidjson::Value::ConstMemberIterator itOverrideMember = overrideValue.MemberBegin(); itOverrideMember != overrideValue.MemberEnd(); ++itOverrideMember) {
        const rapidjson::Value& overrideMemberValue = itOverrideMember->value;
        rapidjson::Value::MemberIterator itSourceMember = sourceValue.FindMember(itOverrideMember->name);
        if (itSourceMember != sourceValue.MemberEnd()) { // found corresponding member in sourceObject
            hasChanged |= UpdateJsonRecursively(itSourceMember->value, overrideMemberValue, alloc);
        }
        else {                  // not found corresponding member in sourceObject, so just copy it
            rapidjson::Value key(itOverrideMember->name, alloc);
            rapidjson::Value val;
            val.CopyFrom(overrideMemberValue, alloc, true);
            sourceValue.AddMember(key, val, alloc);
            hasChanged = true;
        }
    }
    return hasChanged;
}

} // namespace mujinjson

#endif
