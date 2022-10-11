// -*- coding: utf-8 -*-
// Copyright (C) 2016 MUJIN Inc.
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
/** \file mujinexceptions.h
    \brief Exception definitions.
 */
#ifndef MUJIN_EXCEPTIONS_H
#define MUJIN_EXCEPTIONS_H

namespace mujinclient {

#include <mujincontrollerclient/config.h>

/// \brief exception throw when user interrupts the function
class MUJINCLIENT_API UserInterruptException : public std::exception
{
public:
    UserInterruptException() : std::exception() {
    }
    UserInterruptException(const std::string& s) : std::exception(), _s(s) {
    }
    virtual ~UserInterruptException() throw() {
    }
    char const* what() const throw() {
        return _s.c_str();
    }
    const std::string& message() const {
        return _s;
    }

private:
    std::string _s;
};
enum MujinErrorCode {
    MEC_Failed=0,
    MEC_InvalidArguments=1, ///< passed in input arguments are not valid
    MEC_CommandNotSupported=3, ///< string command could not be parsed or is not supported
    MEC_Assert=4,
    MEC_NotInitialized=9, ///< when object is used without it getting fully initialized
    MEC_InvalidState=10, ///< the state of the object is not consistent with its parameters, or cannot be used. This is usually due to a programming error where a vector is not the correct length, etc.
    MEC_Timeout=11, ///< process timed out
    MEC_HTTPClient=12, ///< HTTP client error
    MEC_HTTPServer=13, ///< HTTP server error
    MEC_UserAuthentication=14, ///< authentication failed
    MEC_AlreadyExists=15, ///< the resource already exists and overwriting terminated
    MEC_BinPickingError=16, ///< BinPicking failed
    MEC_HandEyeCalibrationError=17, ///< HandEye Calibration failed
    MEC_ZMQNoResponse=20, ///< No response from the zmq server, using REQ-REP
    MEC_GraphQueryError=21, ///< GraphQL failed to execute
};

inline const char* GetErrorCodeString(MujinErrorCode error)
{
    switch(error) {
    case MEC_Failed: return "Failed";
    case MEC_InvalidArguments: return "InvalidArguments";
    case MEC_CommandNotSupported: return "CommandNotSupported";
    case MEC_Assert: return "Assert";
    case MEC_NotInitialized: return "NotInitialized";
    case MEC_InvalidState: return "InvalidState";
    case MEC_Timeout: return "Timeout";
    case MEC_HTTPClient: return "HTTPClient";
    case MEC_HTTPServer: return "HTTPServer";
    case MEC_UserAuthentication: return "UserAuthentication";
    case MEC_AlreadyExists: return "AlreadyExists";
    case MEC_BinPickingError: return "BinPickingError";
    case MEC_HandEyeCalibrationError: return "HandEyeCalibrationError";
    case MEC_ZMQNoResponse: return "NoResponse";
    case MEC_GraphQueryError: return "GraphQueryError";
    }
    // should throw an exception?
    return "";
}

/// \brief Exception that all Mujin internal methods throw; the error codes are held in \ref MujinErrorCode.
class MUJINCLIENT_API MujinException : public std::exception
{
public:
    MujinException() : std::exception(), _s("unknown exception"), _error(MEC_Failed) {
    }
    MujinException(const std::string& s, MujinErrorCode error=MEC_Failed) : std::exception() {
        _error = error;
        _s = "mujin (";
        _s += GetErrorCodeString(_error);
        _s += "): ";
        _s += s;
    }
    virtual ~MujinException() throw() {
    }
    char const* what() const throw() {
        return _s.c_str();
    }
    const std::string& message() const {
        return _s;
    }
    MujinErrorCode GetCode() const {
        return _error;
    }
private:
    std::string _s;
    MujinErrorCode _error;
};

/// \brief Error that can be thrown by ExecuteGraphQuery API, use GetGraphQueryErrorCode to get detailed error code
class MUJINCLIENT_API MujinGraphQueryError : public MujinException
{
public:
    MujinGraphQueryError(const std::string& s, const std::string& graphQueryErrorCode) : MujinGraphQueryError(s, graphQueryErrorCode.c_str()) {}
    MujinGraphQueryError(const std::string& s, const char* graphQueryErrorCode) : MujinException(s, MEC_GraphQueryError) {
        _graphQueryErrorCode = graphQueryErrorCode;
    }

    /// \brief GraphQL error code, such as "not-found"
    const std::string& GetGraphQueryErrorCode() const {
        return _graphQueryErrorCode;
    }

private:
    std::string _graphQueryErrorCode;    
};

} // namespace mujinclient
#endif
