// -*- coding: utf-8 -*-
// Copyright (C) 2012-2013 MUJIN Inc. <rosen.diankov@mujin.co.jp>
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
/** \file common.h
    \brief  Private common definitions for mujin controller client.
 */
#ifndef MUJIN_CONTROLLERCLIENT_COMMON_H
#define MUJIN_CONTROLLERCLIENT_COMMON_H

#define WIN32_LEAN_AND_MEAN
#include <mujincontrollerclient/mujincontrollerclient.h>

#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/static_assert.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <sstream>
#include <fstream>
#include <iostream>
#include <curl/curl.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#undef GetUserName // classes with ControllerClient::GetUserName

#include <boost/typeof/std/string.hpp>
#include <boost/typeof/std/vector.hpp>
#include <boost/typeof/std/list.hpp>
#include <boost/typeof/std/map.hpp>
#include <boost/typeof/std/set.hpp>
#include <boost/typeof/std/string.hpp>

#define FOREACH(it, v) for(BOOST_TYPEOF(v) ::iterator it = (v).begin(), __itend__=(v).end(); it != __itend__; ++(it))
#define FOREACH_NOINC(it, v) for(BOOST_TYPEOF(v) ::iterator it = (v).begin(), __itend__=(v).end(); it != __itend__; )

#define FOREACHC(it, v) for(BOOST_TYPEOF(v) ::const_iterator it = (v).begin(), __itend__=(v).end(); it != __itend__; ++(it))
#define FOREACHC_NOINC(it, v) for(BOOST_TYPEOF(v) ::const_iterator it = (v).begin(), __itend__=(v).end(); it != __itend__; )

#else

#define FOREACH(it, v) for(typeof((v).begin())it = (v).begin(); it != (v).end(); (it)++)
#define FOREACH_NOINC(it, v) for(typeof((v).begin())it = (v).begin(); it != (v).end(); )

#define FOREACHC FOREACH
#define FOREACHC_NOINC FOREACH_NOINC

//#define FORIT(it, v) for(it = (v).begin(); it != (v).end(); (it)++)

#if BOOST_VERSION >= 104400
// boost filesystem v3 is present after v1.44, so force using it
#define BOOST_FILESYSTEM_VERSION 3
#endif

// only use boost filesystem on linux since it is difficult to get working correctly with windows
#include <boost/filesystem/operations.hpp>
#include <sys/stat.h>
#include <fcntl.h>

#endif // defined(_WIN32) || defined(_WIN64)

#include "utf8.h"

#ifndef MUJIN_TIME
#define MUJIN_TIME
#include <time.h>

#ifndef _WIN32
#if !(defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0))
#include <sys/time.h>
#endif
#else
#include <sys/timeb.h>    // ftime(), struct timeb
inline void usleep(unsigned long microseconds) {
    Sleep((microseconds+999)/1000);
}
#endif

#ifdef _WIN32
inline unsigned long long GetMilliTime()
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (unsigned long long)((count.QuadPart * 1000) / freq.QuadPart);
}

inline unsigned long long GetMicroTime()
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (count.QuadPart * 1000000) / freq.QuadPart;
}

inline unsigned long long GetNanoTime()
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (count.QuadPart * 1000000000) / freq.QuadPart;
}

inline static unsigned long long GetNanoPerformanceTime() {
    return GetNanoTime();
}

#else

inline void GetWallTime(unsigned int& sec, unsigned int& nsec)
{
#if defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0)
    struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);
    sec  = start.tv_sec;
    nsec = start.tv_nsec;
#else
    struct timeval timeofday;
    gettimeofday(&timeofday,NULL);
    sec  = timeofday.tv_sec;
    nsec = timeofday.tv_usec * 1000;
#endif
}

inline unsigned long long GetMilliTimeOfDay()
{
    struct timeval timeofday;
    gettimeofday(&timeofday,NULL);
    return (unsigned long long)timeofday.tv_sec*1000+(unsigned long long)timeofday.tv_usec/1000;
}

inline unsigned long long GetNanoTime()
{
    unsigned int sec,nsec;
    GetWallTime(sec,nsec);
    return (unsigned long long)sec*1000000000 + (unsigned long long)nsec;
}

inline unsigned long long GetMicroTime()
{
    unsigned int sec,nsec;
    GetWallTime(sec,nsec);
    return (unsigned long long)sec*1000000 + (unsigned long long)nsec/1000;
}

inline unsigned long long GetMilliTime()
{
    unsigned int sec,nsec;
    GetWallTime(sec,nsec);
    return (unsigned long long)sec*1000 + (unsigned long long)nsec/1000000;
}

inline static unsigned long long GetNanoPerformanceTime()
{
#if defined(CLOCK_GETTIME_FOUND) && (POSIX_TIMERS > 0 || _POSIX_TIMERS > 0) && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec start;
    unsigned int sec, nsec;
    clock_gettime(CLOCK_MONOTONIC, &start);
    sec  = start.tv_sec;
    nsec = start.tv_nsec;
    return (unsigned long long)sec*1000000000 + (unsigned long long)nsec;
#else
    return GetNanoTime();
#endif
}
#endif
#endif

#ifdef _MSC_VER
#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCDNAME__
#endif
#endif

#define GETCONTROLLERIMPL() ControllerClientImplPtr controller = boost::dynamic_pointer_cast<ControllerClientImpl>(GetController());
#define CHECKCURLCODE(code, msg) if (code != CURLE_OK) { \
        throw MujinException(boost::str(boost::format("[%s:%d] curl function %s with error '%s': %s")%(__PRETTY_FUNCTION__)%(__LINE__)%(msg)%curl_easy_strerror(code)%_errormessage), MEC_HTTPClient); \
}

#define MUJIN_EXCEPTION_FORMAT0(s, errorcode) mujinclient::MujinException(boost::str(boost::format("[%s:%d] " s)%(__PRETTY_FUNCTION__)%(__LINE__)),errorcode)

/// adds the function name and line number to an exception
#define MUJIN_EXCEPTION_FORMAT(s, args,errorcode) mujinclient::MujinException(boost::str(boost::format("[%s:%d] " s)%(__PRETTY_FUNCTION__)%(__LINE__)%args),errorcode)

BOOST_STATIC_ASSERT(sizeof(unsigned short) == 2); // need this for utf-16 reading

namespace mujinclient {

class BinPickingTaskZmqResource;
typedef boost::shared_ptr<BinPickingTaskZmqResource> BinPickingTaskZmqResourcePtr;
typedef boost::weak_ptr<BinPickingTaskZmqResource> BinPickingTaskZmqResourceWeakPtr;

class FileHandler
{
public:
    FileHandler(const char* pfilename) {
        _fd = fopen(pfilename, "rb");
    }
#if defined(_WIN32) || defined(_WIN64)
    FileHandler(const wchar_t* pfilename) {
        _fd = _wfopen(pfilename, L"rb");
    }
#endif
    ~FileHandler() {
        if( !!_fd ) {
            fclose(_fd);
        }
    }
    FILE* _fd;
};

bool PairStringLengthCompare(const std::pair<std::string, std::string>&p0, const std::pair<std::string, std::string>&p1);
std::string& SearchAndReplace(std::string& out, const std::string& in, const std::vector< std::pair<std::string, std::string> >&_pairs);

namespace encoding {

#if defined(_WIN32) || defined(_WIN64)

const std::map<int, std::string>& GetCodePageMap();

inline std::wstring ConvertUTF8toUTF16(const std::string& utf8)
{
    std::wstring utf16(L"");

    if (!utf8.empty()) {
        size_t nLen16 = 0;
        if ((nLen16 = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0)) > 0) {
            utf16.resize(nLen16);
            ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &utf16[0], nLen16);
            // remove the null terminated char that was written
            utf16.resize(nLen16-1);
        }
    }
    return utf16;
}

inline std::string ConvertUTF16toUTF8(const std::wstring& utf16)
{
    std::string utf8("");

    if (!utf16.empty()) {
        size_t nLen8 = 0;
        if ((nLen8 = ::WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, NULL, 0, NULL, NULL)) > 0) {
            utf8.resize(nLen8);
            ::WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &utf8[0], nLen8, NULL, NULL);
            // remove the null terminated char that was written
            utf8.resize(nLen8-1);
        }
    }
    return utf8;
}

inline std::string ConvertUTF16toMBS(const std::wstring& utf16)
{
    std::string mbs("");

    if (!utf16.empty()) {
        size_t nLenA = 0;
        if ((nLenA = ::WideCharToMultiByte(CP_ACP, 0, utf16.c_str(), -1, NULL, 0, NULL, NULL)) > 0) {
            mbs.resize(nLenA);
            ::WideCharToMultiByte(CP_ACP, 0, utf16.c_str(), -1, &mbs[0], nLenA, NULL, NULL);
            // remove the null terminated char that was written
            mbs.resize(nLenA-1);
        }
    }
    return mbs;
}

inline std::wstring ConvertMBStoUTF16(const std::string& mbs)
{
    std::wstring utf16(L"");

    if (!mbs.empty()) {
        size_t nLen16 = 0;
        if ((nLen16 = ::MultiByteToWideChar(CP_ACP, 0, mbs.c_str(), -1, NULL, 0)) > 0) {
            utf16.resize(nLen16);
            ::MultiByteToWideChar(CP_ACP, 0, mbs.c_str(), -1, &utf16[0], nLen16);
            // remove the null terminated char that was written
            utf16.resize(nLen16-1);
        }
    }
    return utf16;
}

inline std::string ConvertMBStoUTF8(const std::string& mbs)
{
    return ConvertUTF16toUTF8(ConvertMBStoUTF16(mbs));
}

inline std::string ConvertUTF8toMBS(const std::string& utf8)
{
    return ConvertUTF16toMBS(ConvertUTF8toUTF16(utf8));
}
#endif // defined(_WIN32) || defined(_WIN64)

/// \brief converts utf-8 encoded string into the encoding  string that the filesystem uses
inline std::string ConvertUTF8ToFileSystemEncoding(const std::string& utf8)
{
#if defined(_WIN32) || defined(_WIN64)
    return encoding::ConvertUTF8toMBS(utf8);
#else
    // most linux systems use utf-8, can use getenv("LANG") to double-check
    return utf8;
#endif
}

/// \brief converts utf-8 encoded string into the encoding  string that the filesystem uses
inline std::string ConvertUTF16ToFileSystemEncoding(const std::wstring& utf16)
{
#if defined(_WIN32) || defined(_WIN64)
    return encoding::ConvertUTF16toMBS(utf16);
#else
    // most linux systems use utf-8, can use getenv("LANG") to double-check
    std::string utf8;
    utf8::utf16to8(utf16.begin(), utf16.end(), std::back_inserter(utf8));
    return utf8;
#endif
}

/// \brief converts utf-8 encoded string into the encoding  string that the filesystem uses
inline std::string ConvertFileSystemEncodingToUTF8(const std::string& fs)
{
#if defined(_WIN32) || defined(_WIN64)
    return encoding::ConvertMBStoUTF8(fs);
#else
    // most linux systems use utf-8, can use getenv("LANG") to double-check
    return fs;
#endif
}

} // end namespace encoding

#if defined(_WIN32) || defined(_WIN64)
const char s_filesep = '\\';
const wchar_t s_wfilesep = L'\\';
#else
const char s_filesep = '/';
const wchar_t s_wfilesep = L'/';
#endif

} // end namespace mujinclient

#endif
