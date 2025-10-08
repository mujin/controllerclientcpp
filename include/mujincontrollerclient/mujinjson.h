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
#include <boost/optional.hpp>
#include <stdint.h>
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <deque>
#include <iostream>

// For backward compatibility
#if __has_include(<optional>) && defined(__cplusplus) && __cplusplus >= 201703L
#include <optional>
#define HAS_STD_OPTIONAL_SUPPORT
#endif

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

class CopyableRapidJsonDocument : public rapidjson::Document
{
public:
    CopyableRapidJsonDocument() = default;

    CopyableRapidJsonDocument(const CopyableRapidJsonDocument& other)
        : rapidjson::Document()
    {
        CopyFrom(other, GetAllocator());
    }

    CopyableRapidJsonDocument& operator=(const CopyableRapidJsonDocument& other) {
        Reset();
        CopyFrom(other, GetAllocator());
        return *this;
    }

    void Reset() {
        SetNull();
        GetAllocator().Clear();
    }
};  // class CopyableRapidJsonDocument

template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
std::string GetJsonString(const rapidjson::GenericValue<Encoding, Allocator>& t);

template<typename T>
inline std::string GetJsonString(const T& t);

/// \brief gets a string of the Value type for debugging purposes
template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline std::string GetJsonTypeName(const rapidjson::GenericValue<Encoding, Allocator>& v) {
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

static constexpr const unsigned int MUJIN_RAPIDJSON_PARSE_FLAGS = rapidjson::kParseDefaultFlags |
                                                                  rapidjson::kParseFullPrecisionFlag | // parse floats in full-precision mode
                                                                  rapidjson::kParseNanAndInfFlag; // Don't error out if we encounter NaN/Inf values
static constexpr const unsigned int MUJIN_RAPIDJSON_WRITE_FLAGS = rapidjson::kWriteDefaultFlags |
                                                                  rapidjson::kWriteNanAndInfFlag; // Allow serialization of Nan/Inf values. Without this, rapidjson will just truncate the stream if it encounters one.

template <typename BufferT>
using MujinRapidJsonWriter = rapidjson::Writer<BufferT, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, MUJIN_RAPIDJSON_WRITE_FLAGS>;
template <typename BufferT>
using MujinRapidJsonPrettyWriter = rapidjson::PrettyWriter<BufferT, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, MUJIN_RAPIDJSON_WRITE_FLAGS>;

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline std::string DumpJson(const rapidjson::GenericValue<Encoding, Allocator>& value, const unsigned int indent=0) {
    rapidjson::StringBuffer stringbuffer;
    if (indent == 0) {
        MujinRapidJsonWriter<rapidjson::StringBuffer> writer(stringbuffer);
        if( !value.Accept(writer) ) {
            throw MujinJSONException("Failed to write json writer");
        }
    } else {
        MujinRapidJsonPrettyWriter<rapidjson::StringBuffer> writer(stringbuffer);
        writer.SetIndent(' ', indent);
        if( !value.Accept(writer) ) {
            throw MujinJSONException("Failed to write json writer");
        }
    }
    return std::string(stringbuffer.GetString(), stringbuffer.GetSize());
}

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void DumpJson(const rapidjson::GenericValue<Encoding, Allocator>& value, std::ostream& os, const unsigned int indent=0) {
    rapidjson::OStreamWrapper osw(os);
    if (indent == 0) {
        MujinRapidJsonWriter<rapidjson::OStreamWrapper> writer(osw);
        if( !value.Accept(writer) ) {
            throw MujinJSONException("Failed to write json writer");
        }
    } else {
        MujinRapidJsonPrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        writer.SetIndent(' ', indent);
        if( !value.Accept(writer) ) {
            throw MujinJSONException("Failed to write json writer");
        }
    }
}

/// \brief this clears the allcoator!
template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void ParseJson(rapidjson::GenericDocument<Encoding, Allocator>& d, const char* str, size_t length)
{
    d.SetNull();
    d.GetAllocator().Clear(); // dangerous if used by other existing objects
    d.template Parse<MUJIN_RAPIDJSON_PARSE_FLAGS>(str, length);
    if (d.HasParseError()) {
        const std::string substr(str, length < 200 ? length : 200);

        throw MujinJSONException(boost::str(boost::format("Json string is invalid (offset %u) %s data is '%s'.")%((unsigned)d.GetErrorOffset())%GetParseError_En(d.GetParseError())%substr));
    }
}

/// \brief this clears the allcoator!
template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void ParseJson(rapidjson::GenericDocument<Encoding, Allocator>& d, const std::string& str) {
    ParseJson(d, str.c_str(), str.size());
}

/// \brief this clears the allcoator!
template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void ParseJson(rapidjson::GenericDocument<Encoding, Allocator>& d, std::istream& is) {
    rapidjson::IStreamWrapper isw(is);
    // see note in: void ParseJson(rapidjson::GenericDocument<Encoding, Allocator>& d, const std::string& str)
    d.SetNull();
    d.GetAllocator().Clear();
    d.template ParseStream<MUJIN_RAPIDJSON_PARSE_FLAGS>(isw);
    if (d.HasParseError()) {
        throw MujinJSONException(boost::str(boost::format("Json stream is invalid (offset %u) %s")%((unsigned)d.GetErrorOffset())%GetParseError_En(d.GetParseError())));
    }
}

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void ParseJson(rapidjson::GenericValue<Encoding, Allocator>& r, Allocator& alloc, std::istream& is)
{
    rapidjson::IStreamWrapper isw(is);
    rapidjson::GenericReader<Encoding, Encoding, Allocator> reader(&alloc);
    //ClearStackOnExit scope(*this);

    size_t kDefaultStackCapacity = 1024;
    rapidjson::GenericDocument<Encoding, Allocator> rTemp(&alloc, kDefaultStackCapacity); // needed by Parse to be a document
    rTemp.template ParseStream<MUJIN_RAPIDJSON_PARSE_FLAGS>(isw);
    if (rTemp.HasParseError()) {
        throw MujinJSONException(boost::str(boost::format("Json stream is invalid (offset %u) %s")%((unsigned)rTemp.GetErrorOffset())%GetParseError_En(rTemp.GetParseError())));
    }
    r.Swap(rTemp);

//    rapidjson::ParseResult parseResult = reader.template Parse<MUJIN_RAPIDJSON_PARSE_FLAGS>(isw, rTemp);
//    if( parseResult.IsError() ) {
//        *stack_.template Pop<ValueType>(1)
//        throw MujinJSONException(boost::str(boost::format("Json stream is invalid (offset %u) %s")%((unsigned)parseResult.Offset())%GetParseError_En(parseResult.Code())));
//    }
}

template <class Container>
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

//template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<>>
//inline void ParseJsonFile(rapidjson::GenericDocument<Encoding, Allocator>& d, const char* filename)
//{
//    std::vector<char> buffer;
//
//    return ParseJsonFile(d, filename, buffer);
//}
//
//template <class Container, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<>>
//void ParseJsonFile(rapidjson::GenericDocument<Encoding, Allocator>& d, const std::string& filename, Container& buffer)
//{
//    return ParseJsonFile(d, filename.c_str(), buffer);
//}
//
//template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<>>
//inline void ParseJsonFile(rapidjson::GenericDocument<Encoding, Allocator>& d, const std::string& filename)
//{
//    std::vector<char> buffer;
//
//    return ParseJsonFile(d, filename.c_str(), buffer);
//}

class JsonSerializable
{
public:
    virtual void LoadFromJson(const rapidjson::Value& v) = 0;
    virtual void SaveToJson(rapidjson::Value& v, rapidjson::Document::AllocatorType& alloc) const = 0;
    virtual void SaveToJson(rapidjson::Document& d) const {
        SaveToJson(d, d.GetAllocator());
    }
    virtual ~JsonSerializable() {
    }
};

template<typename T, class S> inline T LexicalCast(const S& v, const std::string& typeName) {
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
template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, JsonSerializable& t) {
    t.LoadFromJson(v);
}

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::string& t) {
    if (v.IsString()) {
        t.assign(v.GetString(), v.GetStringLength()); // specify size to allow null characters
    } else if (v.IsInt64()) {
        //TODO: add warnings on all usages of lexical_cast
        t = LexicalCast<std::string>(v.GetInt64(), "String");
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to String");
    }

}

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, int& t) {
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

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, int16_t& t) {
    if (v.IsInt()) {
        t = v.GetInt();
    } else if (v.IsString()) {
        t = boost::lexical_cast<int16_t>(v.GetString());
    } else if (v.IsBool()) {
        t = v.GetBool() ? 1 : 0;
    } else {
        throw MujinJSONException("Cannot convert JSON type %s to Int" + GetJsonString(v));
    }
}

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, int8_t& t) {
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

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, uint8_t& t) {
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

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, uint16_t& t) {
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

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, unsigned int& t) {
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

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, unsigned long long& t) {
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

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, uint64_t& t) {
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

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, int64_t& t) {
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

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, double& t) {
    if (v.IsNumber()) {
        t = v.GetDouble();
    } else if (v.IsString()) {
        t = LexicalCast<double>(v.GetString(), "Double");
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to Double");
    }
}

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, float& t) {
    if (v.IsNumber()) {
        t = v.GetDouble();
    } else if (v.IsString()) {
        t = LexicalCast<double>(v.GetString(), "Float");
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to Float");
    }
}

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, bool& t) {
    if (v.IsInt()) t = v.GetInt();
    else if (v.IsBool()) t = v.GetBool();
    else if (v.IsString())  {
        t = LexicalCast<bool>(v.GetString(), "Bool");
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonString(v) + " to Bool");
    }
}

// These prototypes are necessary to allow recursive container types to be deserialized (e.g, std::vector<std::unordered_map<...>>)
// Without declaring these up-front, specializations for value types implemented after the containing type will fail to deduce, causing headaches.

template <typename T, class AllocT = std::allocator<T>, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::vector<T, AllocT>& t);

template <typename T, size_t N, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::array<T, N>& t);

template <typename U, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::map<std::string, U>& t);

template <typename U, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::deque<U>& t);

template <typename U, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::unordered_set<U>& t);

template <typename U, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::set<U>& t);

template <typename U, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::unordered_map<std::string, U>& t);

template <typename T, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, boost::optional<T>& t);

#ifdef HAS_STD_OPTIONAL_SUPPORT
template <typename T, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::optional<T>& t);
#endif

template<typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, boost::shared_ptr<T>& ptr) {
    static_assert(std::is_default_constructible<T>::value, "Shared pointer of type must be default-constructible.");
    ptr = boost::make_shared<T>();
    LoadJsonValue(v, *ptr);
}

template<typename T, typename U, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::pair<T, U>& t) {
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

template<typename T, size_t N, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, T (&p)[N]) {
    if (v.IsArray()) {
        if (v.GetArray().Size() != N) {
            throw MujinJSONException("Json array size doesn't match");
        }
        size_t i = 0;
        for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
            LoadJsonValue(*it, p[i]);
            i++;
        }
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to Array");
    }
}

// default template arguments provided in forward declaration
template<typename T, class AllocT, typename Encoding, typename Allocator>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::vector<T, AllocT>& t) {
    if (v.IsArray()) {
        t.clear();
        t.resize(v.GetArray().Size());
        size_t i = 0;
        for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
            LoadJsonValue(*it, t[i]);
            i++;
        }
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to Vector");
    }
}

// default template arguments provided in forward declaration
template<typename T, size_t N, typename Encoding, typename Allocator>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::array<T, N>& t) {
    if (v.IsArray()) {
        if (v.GetArray().Size() != N) {
            throw MujinJSONException(
                      (boost::format("Cannot convert json type " + GetJsonTypeName(v) + " to Array. "
                                                                                        "Array length does not match (%d != %d)") % N % v.GetArray().Size()).str());
        }
        size_t i = 0;
        for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
            LoadJsonValue(*it, t[i]);
            i++;
        }
    } else {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to Array");
    }
}

template <typename U, typename Encoding, typename Allocator>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::map<std::string, U>& t)
{
    if (v.IsArray()) {
        // list based map
        // TODO: is it dangerous?
        for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstValueIterator itr = v.Begin(); itr != v.End(); ++itr) {
            std::pair<std::string, U> value;
            LoadJsonValue((*itr), value);
            t[value.first] = value.second;
        }
    } else if (v.IsObject()) {
        t.clear();
        for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator it = v.MemberBegin();
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

template <typename U, typename Encoding, typename Allocator>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::deque<U>& t)
{
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
    for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
        // Deserialize directly into the map to avoid copying temporaries.
        LoadJsonValue(*it, t[emplaceIndex++]);
    }
}

template <typename U, typename Encoding, typename Allocator>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::set<U>& t)
{
    // It doesn't make sense to construct a set from anything other than a JSON array
    if (!v.IsArray()) {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to set");
    }

    // Ensure our output is a blank slate
    t.clear();

    // Iterate each array entry and attempt to deserialize it directly as a member type
    for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
        U u;
        LoadJsonValue(*it, u);
        t.emplace(std::move(u));
    }
}

template <typename U, typename Encoding, typename Allocator>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::unordered_set<U>& t)
{
    // It doesn't make sense to construct a set from anything other than a JSON array
    if (!v.IsArray()) {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to set");
    }

    // Ensure our output is a blank slate
    t.clear();
    t.reserve(v.Size());

    // Iterate each array entry and attempt to deserialize it directly as a member type
    for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstValueIterator it = v.Begin(); it != v.End(); ++it) {
        U u;
        LoadJsonValue(*it, u);
        t.emplace(std::move(u));
    }
}

template <typename U, typename Encoding, typename Allocator>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::unordered_map<std::string, U>& t)
{
    // It doesn't make sense to construct an unordered map from anything other
    // than a full JSON object
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot convert json type " + GetJsonTypeName(v) + " to unordered_map");
    }

    // Ensure our output is a blank slate
    t.clear();
    for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator it = v.MemberBegin();
         it != v.MemberEnd(); ++it) {
        // Deserialize directly into the map to avoid copying temporaries.
        // Note that our key needs to be explicitly length-constructed since it
        // may contain \0 bytes.
        LoadJsonValue(
            it->value,
            t[std::string(it->name.GetString(), it->name.GetStringLength())]);
    }
}

template <typename T, typename Encoding, typename Allocator>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, boost::optional<T>& t)
{
    // If the value is null, treat that as an empty optional
    if (v.IsNull()) {
        t.reset();
        return;
    }

    // Otherwise, deserialize as the boxed type
    T temporary;
    LoadJsonValue(v, temporary);
    t = std::move(temporary);
}

#ifdef HAS_STD_OPTIONAL_SUPPORT
template <typename T, typename Encoding, typename Allocator>
inline void LoadJsonValue(const rapidjson::GenericValue<Encoding, Allocator>& v, std::optional<T>& t)
{
    // If the value is null, treat that as an empty optional
    if (v.IsNull()) {
        t.reset();
        return;
    }

    // Otherwise, deserialize as the boxed type
    T temporary;
    LoadJsonValue(v, temporary);
    t = std::move(temporary);
}
#endif

//Save a data structure to rapidjson::GenericValue<Encoding, Allocator> format

/*template<typename T> inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const T& t, rapidjson::GenericDocument<Encoding, Allocator>::AllocatorType& alloc) {*/
/*JsonWrapper<T>::SaveToJson(v, t, alloc);*/
/*}*/

template <typename Encoding, typename Allocator, typename Allocator2 >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const JsonSerializable& t, Allocator2& alloc) {
    t.SaveToJson(v, alloc);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::string& t, Allocator2& alloc) {
    v.SetString(t.c_str(), alloc);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const char* t, Allocator2& alloc) {
    v.SetString(t, alloc);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const int& t, Allocator2& alloc) {
    v.SetInt(t);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const unsigned int& t, Allocator2& alloc) {
    v.SetUint(t);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const long long& t, Allocator2& alloc) {
    v.SetInt64(t);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const int64_t& t, Allocator2& alloc) {
    v.SetInt64(t);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const unsigned long long& t, Allocator2& alloc) {
    v.SetUint64(t);
}

#ifndef _MSC_VER
template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const uint64_t& t, Allocator2& alloc) {
    v.SetUint64(t);
}
#endif

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const bool& t, Allocator2& alloc) {
    v.SetBool(t);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const int8_t& t, Allocator2& alloc) {
    v.SetInt(t);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const double& t, Allocator2& alloc) {
    v.SetDouble(t);
}

template <typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const float& t, Allocator2& alloc) {
    v.SetDouble(t);
}

template <typename Encoding, typename Allocator, typename Encoding2, typename Allocator2, typename Allocator3>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const rapidjson::GenericValue<Encoding2, Allocator2>& t, Allocator3& alloc)
{
    v.CopyFrom(t, alloc);
}


template <typename Encoding, typename Allocator>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const CopyableRapidJsonDocument& t, Allocator& alloc) {
    v.CopyFrom(t, alloc);
}

// These prototypes are necessary to allow recursive container types to be serialized (e.g, std::vector<std::unordered_map<...>>)
// Without declaring these up-front, specializations for value types implemented after the containing type will fail to deduce, causing headaches.

template <typename T, class AllocT = std::allocator<T>, typename Encoding = rapidjson::UTF8<>, typename Allocator = rapidjson::MemoryPoolAllocator<>, typename Allocator2 = rapidjson::MemoryPoolAllocator<>>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::vector<T, AllocT>& t, Allocator2& alloc);

template <typename T, size_t N, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::array<T, N>& t, Allocator2& alloc);

template <typename U, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::map<std::string, U>& t, Allocator2& alloc);

template <typename U, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::unordered_map<std::string, U>& t, Allocator2& alloc);

template <typename T, class AllocT, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::deque<T, AllocT>& t, Allocator2& alloc);

template <typename T, class AllocT, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::set<T, AllocT>& t, Allocator2& alloc);

template <typename T, class AllocT, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::unordered_set<T, AllocT>& t, Allocator2& alloc);

template <typename T, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const boost::optional<T>& t, Allocator2& alloc);

#ifdef HAS_STD_OPTIONAL_SUPPORT
template <typename T, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::optional<T>& t, Allocator2& alloc);
#endif

/** do not remove: otherwise boost::shared_ptr could be treated as bool
 */
template<typename T, typename Encoding, typename Allocator, typename Allocator2 >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const boost::shared_ptr<T>& ptr, Allocator2& alloc) {
    SaveJsonValue(v, *ptr, alloc);
}

template<typename T, typename U, typename Encoding, typename Allocator, typename Allocator2 >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::pair<T, U>& t, Allocator2& alloc) {
    v.SetArray();
    v.Reserve(2, alloc);
    rapidjson::GenericValue<Encoding, Allocator> rFirst;
    SaveJsonValue(rFirst, t.first, alloc);
    v.PushBack(rFirst, alloc);
    rapidjson::GenericValue<Encoding, Allocator> rSecond;
    SaveJsonValue(rSecond, t.second, alloc);
    v.PushBack(rSecond, alloc);
}

// default template arguments provided in forward declaration
template<typename T, class AllocT, typename Encoding, typename Allocator, typename Allocator2 >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::vector<T, AllocT>& t, Allocator2& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (size_t i = 0; i < t.size(); ++i) {
        rapidjson::GenericValue<Encoding, Allocator> tmpv;
        SaveJsonValue(tmpv, t[i], alloc);
        v.PushBack(tmpv, alloc);
    }
}

template<typename T, size_t N, typename Encoding, typename Allocator, typename Allocator2 >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::array<T, N>& t, Allocator2& alloc) {
    v.SetArray();
    v.Reserve(N, alloc);
    for (size_t i = 0; i < N; ++i) {
        rapidjson::GenericValue<Encoding, Allocator> tmpv;
        SaveJsonValue(tmpv, t[i], alloc);
        v.PushBack(tmpv, alloc);
    }
}

template<class AllocT = std::allocator<double>, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<>, typename Allocator2=rapidjson::MemoryPoolAllocator<> >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::vector<double, AllocT>& t, Allocator2& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (size_t i = 0; i < t.size(); ++i) {
        v.PushBack(t[i], alloc);
    }
}

template <typename T, size_t N, typename Encoding, typename Allocator, typename Allocator2,
          typename = typename std::enable_if<!std::is_same<char, T>::value>::type> // Disable instantiation for char[], since we want to handle those as strings
void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const T (&t)[N], Allocator2& alloc)
{
    v.SetArray();
    v.Reserve(N, alloc);
    for (size_t i = 0; i < N; ++i) {
        rapidjson::GenericValue<Encoding, Allocator> tmpv;
        SaveJsonValue(tmpv, t[i], alloc);
        v.PushBack(tmpv, alloc);
    }
}

template<size_t N, typename Encoding, typename Allocator, typename Allocator2 >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const double (&t)[N], Allocator2& alloc) {
    v.SetArray();
    v.Reserve(N, alloc);
    for (size_t i = 0; i < N; ++i) {
        v.PushBack(t[i], alloc);
    }
}

template<size_t N, typename Encoding, typename Allocator, typename Allocator2 >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::array<double, N>& t, Allocator2& alloc) {
    v.SetArray();
    v.Reserve(N, alloc);
    for (size_t i = 0; i < N; ++i) {
        v.PushBack(t[i], alloc);
    }
}

template<typename U, typename Encoding, typename Allocator, typename Allocator2 >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::map<std::string, U>& t, Allocator2& alloc) {
    v.SetObject();
    for (typename std::map<std::string, U>::const_iterator it = t.begin(); it != t.end(); ++it) {
        rapidjson::GenericValue<Encoding, Allocator> name, value;
        SaveJsonValue(name, it->first, alloc);
        SaveJsonValue(value, it->second, alloc);
        v.AddMember(name, value, alloc);
    }
}

template<typename U, typename Encoding, typename Allocator, typename Allocator2 >
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::unordered_map<std::string, U>& t, Allocator2& alloc) {
    v.SetObject();
    for (typename std::unordered_map<std::string, U>::const_iterator it = t.begin(); it != t.end(); ++it) {
        rapidjson::GenericValue<Encoding, Allocator> name, value;
        SaveJsonValue(name, it->first, alloc);
        SaveJsonValue(value, it->second, alloc);
        v.AddMember(name, value, alloc);
    }
}

template <typename T, class AllocT, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::deque<T, AllocT>& t, Allocator2& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (typename std::deque<T, AllocT>::const_iterator it = t.begin(); it != t.end(); ++it) {
        rapidjson::GenericValue<Encoding, Allocator> value;
        SaveJsonValue(value, *it, alloc);
        v.PushBack(value, alloc);
    }
}

template <typename T, class AllocT, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::set<T, AllocT>& t, Allocator2& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (typename std::set<T, AllocT>::const_iterator it = t.begin(); it != t.end(); ++it) {
        rapidjson::GenericValue<Encoding, Allocator> value;
        SaveJsonValue(value, *it, alloc);
        v.PushBack(value, alloc);
    }
}

template <typename T, class AllocT, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::unordered_set<T, AllocT>& t, Allocator2& alloc) {
    v.SetArray();
    v.Reserve(t.size(), alloc);
    for (typename std::unordered_set<T, AllocT>::const_iterator it = t.begin(); it != t.end(); ++it) {
        rapidjson::GenericValue<Encoding, Allocator> value;
        SaveJsonValue(value, *it, alloc);
        v.PushBack(value, alloc);
    }
}

template<typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void SaveJsonValue(rapidjson::GenericDocument<Encoding, Allocator>& v, const T& t) {
    // rapidjson::GenericValue<Encoding, Allocator>::CopyFrom also doesn't free up memory, need to clear memory
    // see note in: void ParseJson(rapidjson::GenericDocument<Encoding, Allocator>& d, const std::string& str)
    v.SetNull();
    v.GetAllocator().Clear();
    SaveJsonValue(v, t, v.GetAllocator());
}

template <typename T, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const boost::optional<T>& t, Allocator2& alloc) {
    // If the optional has no value, count that as null
    if (!t.has_value()) {
        v.SetNull();
        return;
    }

    // Otherwise, serialize as the boxed type
    SaveJsonValue(v, *t, alloc);
}

#ifdef HAS_STD_OPTIONAL_SUPPORT
template <typename T, typename Encoding, typename Allocator, typename Allocator2>
inline void SaveJsonValue(rapidjson::GenericValue<Encoding, Allocator>& v, const std::optional<T>& t, Allocator2& alloc) {
    // If the optional has no value, count that as null
    if (!t.has_value()) {
        v.SetNull();
        return;
    }

    SaveJsonValue(v, t.value(), alloc);
}
#endif

//get one json value by key, and store it in local data structures
//return true if key is present. Will return false if the key is not present or the member is Null
template<typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
bool inline LoadJsonValueByKey(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, T& t) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot load value of non-object.");
    }
    typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator itMember = v.FindMember(key);
    if( itMember != v.MemberEnd() ) {
        const rapidjson::GenericValue<Encoding, Allocator>& rMember = itMember->value;
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

template<typename T, typename U, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValueByKey(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, T& t, const U& d) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot load value of non-object.");
    }

    typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator itMember = v.FindMember(key);
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

template<typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValueByPath(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, T& t) {
    const rapidjson::GenericValue<Encoding, Allocator> *child = rapidjson::Pointer(key).Get(v);
    if (child && !child->IsNull()) {
        LoadJsonValue(*child, t);
    }
}

template<typename T, typename U, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void LoadJsonValueByPath(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, T& t, const U& d) {
    const rapidjson::GenericValue<Encoding, Allocator> *child = rapidjson::Pointer(key).Get(v);
    if (child && !child->IsNull()) {
        LoadJsonValue(*child, t);
    }
    else {
        t = d;
    }
}


//work the same as LoadJsonValueByKey, but the value is returned instead of being passed as reference
template<typename T, typename U, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
T GetJsonValueByKey(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, const U& t) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot get value of non-object.");
    }

    typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator itMember = v.FindMember(key);
    if (itMember != v.MemberEnd() ) {
        const rapidjson::GenericValue<Encoding, Allocator>& child = itMember->value;
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

template <typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline T GetJsonValueByKey(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot load value of non-object.");
    }
    T r = T();
    typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator itMember = v.FindMember(key);
    if (itMember != v.MemberEnd() ) {
        const rapidjson::GenericValue<Encoding, Allocator>& child = itMember->value;
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

template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline std::string GetStringJsonValueByKey(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, const std::string& defaultValue=std::string()) {
    return GetJsonValueByKey<std::string, std::string>(v, key, defaultValue);
}

/// \brief default value is returned when there is no key or value is null
template <typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline const char* GetCStringJsonValueByKey(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, const char* pDefaultValue=nullptr) {
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot load value of non-object.");
    }
    typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator itMember = v.FindMember(key);
    if (itMember != v.MemberEnd() ) {
        const rapidjson::GenericValue<Encoding, Allocator>& child = itMember->value;
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

template<typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline T GetJsonValueByPath(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key) {
    T r;
    const rapidjson::GenericValue<Encoding, Allocator> *child = rapidjson::Pointer(key).Get(v);
    if (child && !child->IsNull()) {
        LoadJsonValue(*child, r);
    }
    return r;
}

template<typename T, typename U, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
T GetJsonValueByPath(const rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, const U& t) {
    const rapidjson::GenericValue<Encoding, Allocator> *child = rapidjson::Pointer(key).Get(v);
    if (child && !child->IsNull()) {
        T r;
        LoadJsonValue(*child, r);
        return r;
    }
    else {
        return T(t);
    }
}

template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline const rapidjson::GenericValue<Encoding, Allocator>* GetJsonPointerByPath(const rapidjson::GenericValue<Encoding, Allocator>& value)
{
    return &value;
}

template <size_t N, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<>, typename ... Ps>
const rapidjson::GenericValue<Encoding, Allocator>* GetJsonPointerByPath(const rapidjson::GenericValue<Encoding, Allocator>& value, const char (& key)[N], Ps&&... ps)
{
    const typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator it = value.FindMember(rapidjson::GenericValue<Encoding, Allocator>(key, N - 1));
    if (it != value.MemberEnd()) {
        return GetJsonPointerByPath(it->value, static_cast<Ps&&>(ps)...);
    } else {
        return nullptr;
    }
}

template<typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void SetJsonValueByPath(rapidjson::GenericValue<Encoding, Allocator>& d, const char* path, const T& t, Allocator& alloc) {
    rapidjson::GenericValue<Encoding, Allocator> v;
    SaveJsonValue(v, t, alloc);
    rapidjson::Pointer(path).Swap(d, v, alloc);
}

template<typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void SetJsonValueByPath(rapidjson::GenericDocument<Encoding, Allocator>& d, const char* path, const T& t) {
    SetJsonValueByPath(d, path, t, d.GetAllocator());
}

template<typename T, typename Encoding, typename Allocator, typename Encoding2, typename Allocator2, typename Allocator3>
inline void SetJsonValueByKey(rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, const rapidjson::GenericValue<Encoding2, Allocator2>& t, Allocator3& alloc)
{
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot set value for non-object.");
    }
    if (v.HasMember(key)) {
        SaveJsonValue(v[key], t, alloc);
    }
    else {
        rapidjson::GenericValue<Encoding, Allocator> value, name;
        SaveJsonValue(name, key, alloc);
        SaveJsonValue(value, t, alloc);
        v.AddMember(name, value, alloc);
    }
}

template<typename T, typename Encoding, typename Allocator, typename Encoding2, typename Allocator2, typename Allocator3>
inline void SetJsonValueByKey(rapidjson::GenericValue<Encoding, Allocator>& v, const std::string& key, const rapidjson::GenericValue<Encoding2, Allocator2>& t, Allocator3& alloc)
{
    SetJsonValueByKey(v, key.c_str(), t, alloc);
}

template<typename T, typename Encoding, typename Allocator, typename Allocator2>
inline void SetJsonValueByKey(rapidjson::GenericValue<Encoding, Allocator>& v, const char* key, const T& t, Allocator2& alloc)
{
    if (!v.IsObject()) {
        throw MujinJSONException("Cannot set value for non-object.");
    }
    if (v.HasMember(key)) {
        SaveJsonValue(v[key], t, alloc);
    }
    else {
        rapidjson::GenericValue<Encoding, Allocator> value, name;
        SaveJsonValue(name, key, alloc);
        SaveJsonValue(value, t, alloc);
        v.AddMember(name, value, alloc);
    }
}

template<typename T, typename Encoding, typename Allocator, typename Allocator2>
inline void SetJsonValueByKey(rapidjson::GenericValue<Encoding, Allocator>& v, const std::string& key, const T& t, Allocator2& alloc)
{
    SetJsonValueByKey(v, key.c_str(), t, alloc);
}

template<typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void SetJsonValueByKey(rapidjson::GenericDocument<Encoding, Allocator>& d, const char* key, const T& t)
{
    SetJsonValueByKey(d, key, t, d.GetAllocator());
}

template<typename T, typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void SetJsonValueByKey(rapidjson::GenericDocument<Encoding, Allocator>& d, const std::string& key, const T& t)
{
    SetJsonValueByKey(d, key.c_str(), t, d.GetAllocator());
}

template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void ValidateJsonString(const std::string& str) {
    rapidjson::GenericDocument<Encoding, Allocator> d;
    if (d.Parse(str.c_str()).HasParseError()) {
        throw MujinJSONException("json string " + str + " is invalid." + GetParseError_En(d.GetParseError()));
    }
}

// default template arguments provided in forward declaration
template<typename Encoding, typename Allocator>
inline std::string GetJsonString(const rapidjson::GenericValue<Encoding, Allocator>& t) {
    rapidjson::GenericDocument<Encoding, Allocator> d;
    SaveJsonValue(d, t, d.GetAllocator());
    return DumpJson(d);
}

template<typename T>
inline std::string GetJsonString(const T& t) {
    rapidjson::Document d;
    SaveJsonValue(d, t, d.GetAllocator());
    return DumpJson(d);
}

template<typename T, typename U>
inline std::string GetJsonStringByKey(const U& key, const T& t) {
    rapidjson::Document d(rapidjson::kObjectType);
    SetJsonValueByKey(d, key, t);
    return DumpJson(d);
}

/** update a json object with another one, new key-value pair will be added, existing ones will be overwritten
 */
template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline void UpdateJson(rapidjson::GenericDocument<Encoding, Allocator>& a, const rapidjson::GenericValue<Encoding, Allocator>& b) {
    if (!a.IsObject()) {
        throw MujinJSONException("json object should be a dict to be updated: " + GetJsonString(a));
    }
    if (!b.IsObject()) {
        throw MujinJSONException("json object should be a dict to update another dict: " + GetJsonString(b));
    }
    for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator it = b.MemberBegin(); it != b.MemberEnd(); ++it) {
        SetJsonValueByKey(a, it->name.GetString(), it->value);
    }
}

/// \brief One way merge overrideValue into sourceValue. sourceValue will be overwritten. When both sourceValue and overrideValue are objects and a member of overrideValue is a rapidjson object and the corresponding member of sourceValue is also a rapidjson object, call this function recursivelly inside this function.
/// \param sourceValue json value to be updated
/// \param overrideValue read-only json value used to update contents of sourceValue
/// \return true if sourceValue has changed
template<typename Encoding=rapidjson::UTF8<>, typename Allocator=rapidjson::MemoryPoolAllocator<> >
inline bool UpdateJsonRecursively(rapidjson::GenericValue<Encoding, Allocator>& sourceValue, const rapidjson::GenericValue<Encoding, Allocator>& overrideValue, Allocator& alloc) {
    if (sourceValue == overrideValue) {
        return false;
    }

    if (!sourceValue.IsObject() || !overrideValue.IsObject()) {
        sourceValue.CopyFrom(overrideValue, alloc, true);
        return true;
    }

    bool hasChanged = false;
    for (typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator itOverrideMember = overrideValue.MemberBegin(); itOverrideMember != overrideValue.MemberEnd(); ++itOverrideMember) {
        const rapidjson::GenericValue<Encoding, Allocator>& overrideMemberValue = itOverrideMember->value;
        typename rapidjson::GenericValue<Encoding, Allocator>::MemberIterator itSourceMember = sourceValue.FindMember(itOverrideMember->name);
        if (itSourceMember != sourceValue.MemberEnd()) { // found corresponding member in sourceObject
            hasChanged |= UpdateJsonRecursively(itSourceMember->value, overrideMemberValue, alloc);
        }
        else {                  // not found corresponding member in sourceObject, so just copy it
            rapidjson::GenericValue<Encoding, Allocator> key(itOverrideMember->name, alloc);
            rapidjson::GenericValue<Encoding, Allocator> val;
            val.CopyFrom(overrideMemberValue, alloc, true);
            sourceValue.AddMember(key, val, alloc);
            hasChanged = true;
        }
    }
    return hasChanged;
}

} // namespace mujinjson

#endif
