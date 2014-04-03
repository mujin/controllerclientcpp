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
/** \file mujincontrollerclient.h
    \brief  Defines the public headers of the MUJIN Controller Client
 */
#ifndef MUJIN_CONTROLLERCLIENT_H
#define MUJIN_CONTROLLERCLIENT_H

#ifndef MUJINCLIENT_DISABLE_ASSERT_HANDLER
#define BOOST_ENABLE_ASSERT_HANDLER
#endif

#ifdef _MSC_VER

#pragma warning(disable:4251) // needs to have dll-interface to be used by clients of class
#pragma warning(disable:4190) // C-linkage specified, but returns UDT 'boost::shared_ptr<T>' which is incompatible with C
#pragma warning(disable:4819) //The file contains a character that cannot be represented in the current code page (932). Save the file in Unicode format to prevent data loss using native typeof

#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCDNAME__
#endif

#else
#endif

#if defined(__GNUC__)
#define MUJINCLIENT_DEPRECATED __attribute__((deprecated))
#else
#define MUJINCLIENT_DEPRECATED
#endif

#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <string>
#include <exception>

#include <iomanip>
#include <fstream>
#include <sstream>

#include <boost/version.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/format.hpp>
#include <boost/array.hpp>

#define FOREACH(it, v) for(BOOST_TYPEOF(v) ::iterator it = (v).begin(); it != (v).end(); (it)++)
#define FOREACH_NOINC(it, v) for(BOOST_TYPEOF(v) ::iterator it = (v).begin(); it != (v).end(); )

#define FOREACHC(it, v) for(BOOST_TYPEOF(v) ::const_iterator it = (v).begin(); it != (v).end(); (it)++)
#define FOREACHC_NOINC(it, v) for(BOOST_TYPEOF(v) ::const_iterator it = (v).begin(); it != (v).end(); )

namespace mujinclient {

#include <mujincontrollerclient/config.h>

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
    MEC_HandEyeCalibrationError=17 ///< HandEye Calibration failed
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

class ControllerClient;
class ObjectResource;
class RobotResource;
class SceneResource;
class TaskResource;
class OptimizationResource;
class PlanningResultResource;

typedef boost::shared_ptr<ControllerClient> ControllerClientPtr;
typedef boost::weak_ptr<ControllerClient> ControllerClientWeakPtr;
typedef boost::shared_ptr<ObjectResource> ObjectResourcePtr;
typedef boost::weak_ptr<ObjectResource> ObjectResourceWeakPtr;
typedef boost::shared_ptr<RobotResource> RobotResourcePtr;
typedef boost::weak_ptr<RobotResource> RobotResourceWeakPtr;
typedef boost::shared_ptr<SceneResource> SceneResourcePtr;
typedef boost::weak_ptr<SceneResource> SceneResourceWeakPtr;
typedef boost::shared_ptr<TaskResource> TaskResourcePtr;
typedef boost::weak_ptr<TaskResource> TaskResourceWeakPtr;
typedef boost::shared_ptr<OptimizationResource> OptimizationResourcePtr;
typedef boost::weak_ptr<OptimizationResource> OptimizationResourceWeakPtr;
typedef boost::shared_ptr<PlanningResultResource> PlanningResultResourcePtr;
typedef boost::weak_ptr<PlanningResultResource> PlanningResultResourceWeakPtr;
typedef double Real;

/// \brief status code for a job
///
/// Definitions are very similar to http://ros.org/doc/api/actionlib_msgs/html/msg/GoalStatus.html
enum JobStatusCode {
    JSC_Pending         = 0, ///< The goal has yet to be processed
    JSC_Active          = 1, ///< The goal is currently being processed
    JSC_Preempted       = 2, ///< The goal received a cancel request after it started executing and has since completed its execution.
    JSC_Succeeded       = 3, ///< The goal was achieved successfully.
    JSC_Aborted         = 4, ///< The goal was aborted during execution due to some failure
    JSC_Rejected        = 5, ///< The goal was rejected without being processed, because the goal was unattainable or invalid.
    JSC_Preempting      = 6, ///< The goal received a cancel request after it started executing and has not yet completed execution
    JSC_Recalling       = 7, ///< The goal received a cancel request before it started executing, but the server has not yet confirmed that the goal is canceled.
    JSC_Recalled        = 8, ///< The goal received a cancel request before it started executing and was successfully cancelled
    JSC_Lost            = 9, ///< An error happened and the job stopped being tracked.
    JSC_Unknown = 0xffffffff, ///< the job is unknown
};

struct JobStatus
{
    JobStatus() : code(JSC_Unknown) {
    }
    JobStatusCode code; ///< status code on whether the job is active
    std::string type; ///< the type of job running
    std::string message; ///< current message of the job
    double elapsedtime; ///< how long the job has been running for in seconds
    std::string pk; ///< the primary key to differentiate this job
};

/// \brief an affine transform
struct Transform
{
    Transform() {
        quaternion[0] = 1; quaternion[1] = 0; quaternion[2] = 0; quaternion[3] = 0;
        translate[0] = 0; translate[1] = 0; translate[2] = 0;
    }
    Real quaternion[4]; ///< quaternion [cos(ang/2), axis*sin(ang/2)]
    Real translate[3]; ///< translation x,y,z
};

struct InstanceObjectState
{
    Transform transform; ///< the transform of this instance object
    std::vector<Real> dofvalues; ///< the joint values
};

typedef std::map<std::string, InstanceObjectState> EnvironmentState;

struct SceneInformation
{
    std::string pk; ///< primary key
    std::string uri; ///< uri of the file pointed to
    std::string datemodified; ///< late date modified
    std::string name;
};

/// \brief holds information about the itlplanning task parameters
class ITLPlanningTaskParameters
{
public:
    ITLPlanningTaskParameters() {
        SetDefaults();
    }
    virtual void SetDefaults() {
        startfromcurrent = 0;
        returntostart = 1;
        vrcruns = 1;
        ignorefigure = 1;
        unit = "mm";
        optimizationvalue = 1;
        program.clear();
    }
    int startfromcurrent; ///< Will start planning from the current robot joint values, otherwise will start at the first waypoint in the program.
    int returntostart; ///< Plan the return path of the robot to where the entire trajectory started. Makes it possible to loop the robot motion.
    int vrcruns; ///< Use the Robot Virtual Controller for retiming and extra validation. Makes planning slow, but robot timing because very accurate.
    int ignorefigure; ///< if 1, ignores the figure/structure flags for every goal parameter. These flags fix the configuration of the robot from the multitute of possibilities. If 0, will attempt to use the flags and error if task is not possible with them.
    std::string unit; ///< the unit that information is used in. m, mm, nm, inch, etc
    Real optimizationvalue; ///< value in [0,1]. 0 is no optimization (fast), 1 is full optimization (slow)
    std::string program; ///< itl program
};

/// program is wincaps rc8 pac script
class DensoWaveWincapsTaskParameters : public ITLPlanningTaskParameters
{
public:
    DensoWaveWincapsTaskParameters() : ITLPlanningTaskParameters() {
        SetDefaults();
    }
    void SetDefaults() {
        ITLPlanningTaskParameters::SetDefaults();
        preservespeedparameters = 0;
    }
    int preservespeedparameters; ///< if 1, preserves all SPEED/ACCEL commands as best as possible.
};

/// \brief placement optimization for a robot or another target.
struct RobotPlacementOptimizationParameters
{
    RobotPlacementOptimizationParameters() {
        SetDefaults();
    }
    inline void SetDefaults() {
        unit = "mm";
        topstorecandidates = 20;
        targetname.clear();
        framename.clear();
        minrange[0] = -400; minrange[1] = -400; minrange[2] = 0; minrange[3] = -180;
        maxrange[0] = 400; maxrange[1] = 400; maxrange[2] = 400; maxrange[3] = 90;
        stepsize[0] = 100; stepsize[1] = 100; stepsize[2] = 100; stepsize[3] = 90;
        ignorebasecollision = 0;
    }
    std::string targetname; ///< the name of the target object to optimize for. If blank, will use robot. Has to start with "0 instobject " for environment inst object targets.
    std::string framename; ///< The name of the frame to define the optimization parameters in. If blank, will use the targetname's coordinate system. Has to start iwth "0 instobject " for environment inst object frames.
    Real maxrange[4]; ///< X, Y, Z, Angle (deg)
    Real minrange[4]; ///< X, Y, Z, Angle (deg)
    Real stepsize[4]; ///< X, Y, Z, Angle (deg)
    int ignorebasecollision; ///< If 1, when moving the robot, allow collisions of the base with the environment, this allows users to search for a base placement and while ignoring small obstacles. By default this is 0.

    std::string unit; ///< the unit that information is used in. m, mm, nm, inch, etc
    int topstorecandidates; ///< In order to speed things up, store at least the top (fastest) N candidates. Candidates beyond the top N will not be computed.
};

struct PlacementsOptimizationParameters
{
    PlacementsOptimizationParameters() {
        SetDefaults();
    }
    inline void SetDefaults() {
        unit = "mm";
        topstorecandidates = 20;
        for(size_t itarget = 0; itarget < targetnames.size(); ++itarget) {
            targetnames[itarget].clear();
            framenames[itarget].clear();
            ignorebasecollisions[itarget] = 0;
        }
        minranges[0][0] = -400; minranges[0][1] = -400; minranges[0][2] = 0; minranges[0][3] = -180;
        maxranges[0][0] = 400; maxranges[0][1] = 400; maxranges[0][2] = 400; maxranges[0][3] = 90;
        stepsizes[0][0] = 100; stepsizes[0][1] = 100; stepsizes[0][2] = 100; stepsizes[0][3] = 90;
        minranges[1][0] = -100; minranges[1][1] = -100; minranges[1][2] = 0; minranges[1][3] = 0;
        maxranges[1][0] = 100; maxranges[1][1] = 100; maxranges[1][2] = 100; maxranges[1][3] = 0;
        stepsizes[1][0] = 100; stepsizes[1][1] = 100; stepsizes[1][2] = 100; stepsizes[1][3] = 90;
    }

    // for every target, there's one setting:
    boost::array<std::string, 2> targetnames; ///< the primary key of the target object to optimize for. If blank, will use robot. Has to start with "0 instobject " for environment inst object targets.
    boost::array<std::string, 2> framenames; ///< The primary key of the frame to define the optimization parameters in. If blank, will use the target's coordinate system. Has to start iwth "0 instobject " for environment inst object frames.
    boost::array<Real[4],2> maxranges; ///< X, Y, Z, Angle (deg)
    boost::array<Real[4],2> minranges; ///< X, Y, Z, Angle (deg)
    boost::array<Real[4],2> stepsizes; ///< X, Y, Z, Angle (deg)
    boost::array<int,2> ignorebasecollisions; ///< If 1, when moving the robot, allow collisions of the base with the environment, this allows users to search for a base placement and while ignoring small obstacles. By default this is 0.

    // shared settings
    std::string unit; ///< the unit that information is used in. m, mm, nm, inch, etc
    int topstorecandidates; ///< In order to speed things up, store at least the top (fastest) N candidates. Candidates beyond the top N will not be computed.
};

/// \brief program data for an individual robot
class RobotProgramData
{
public:
    RobotProgramData() {
    }
    RobotProgramData(const std::string& programdata, const std::string& type) : programdata(programdata), type(type) {
    }
    std::string programdata; ///< the program data
    std::string type; ///< the type of program
};

/// \brief program data for all robots.
class RobotControllerPrograms
{
public:
    std::map<std::string, RobotProgramData> programs; ///< the keys are the robot instance primary keys of the scene
};

/// \brief Creates on MUJIN Controller instance.
///
/// Only one call can be made at a time. In order to make multiple calls simultaneously, create another instance.
class MUJINCLIENT_API ControllerClient
{
public:
    virtual ~ControllerClient() {
    }

//    \brief Returns a list of filenames in the user system of a particular type
//
//        \param scenetype the type of scene possible values are:
//        - mujincollada
//        - wincaps
//        - rttoolbox
//        - cecrobodiaxml
//        - stl
//        - x
//        - vrml
//        - stl
//
//    virtual void GetSceneFilenames(const std::string& scenetype, std::vector<std::string>& scenefilenames) = 0;

    /// \brief sets the character encoding for all strings that are being input and output from the resources
    ///
    /// The default character encoding is \b utf-8, can also set it to \b Shift_JIS for windows japanese unicode, \b iso-2022-jp
    /// List of possible sets: http://www.iana.org/assignments/character-sets/character-sets.xml
    virtual void SetCharacterEncoding(const std::string& newencoding) = 0;

    /// \brief sets the language code for all output
    ///
    /// Check out http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
    virtual void SetLanguage(const std::string& language) = 0;

    /// \brief If necessary, changes the proxy to communicate to the controller server
    ///
    /// \param serverport Specify proxy server to use. To specify port number in this string, append :[port] to the end of the host name. The proxy string may be prefixed with [protocol]:// since any such prefix will be ignored. The proxy's port number may optionally be specified with the separate option. If not specified, will default to using port 1080 for proxies. Setting to empty string will disable the proxy.
    /// \param userpw If non-empty, [user name]:[password] to use for the connection to the HTTP proxy.
    virtual void SetProxy(const std::string& serverport, const std::string& userpw) = 0;

    /// \brief Restarts the MUJIN Controller Server and destroys any optimizaiton jobs.
    ///
    /// If the server is not responding, call this method to clear the server state and initialize everything.
    /// The method is blocking, when it returns the MUJIN Controller would have been restarted.
    virtual void RestartServer() = 0;

    /// \brief Upgrade the controller with this data
    virtual void Upgrade(const std::vector<unsigned char>& vdata) = 0;

    /// \brief returns the mujin controller version
    virtual std::string GetVersion() = 0;

    /// \brief sends the cancel message to all jobs.
    ///
    /// The method is non-blocking
    virtual void CancelAllJobs() = 0;

    /// \brief get all the run-time statuses
    ///
    /// \param options if options is 1, also get the message
    virtual void GetRunTimeStatuses(std::vector<JobStatus>& statuses, int options=0) = 0;

    /// \brief gets a list of all the scene primary keys currently available to the user
    virtual void GetScenePrimaryKeys(std::vector<std::string>& scenekeys) = 0;

    /** \brief Register a scene to be used by the MUJIN Controller

        \param uri utf-8 encoded URI of the file on the MUJIN Controller to import. Usually starts with \b mujin:/
        \param scenetype The format of the source file. Can be:
        - **mujincollada**
        - **wincaps** (DensoWave WINCAPS III)
        - **rttoolbox** (Mitsubishi RT ToolBox)
        - **x** (DirectX)
        - **vrml**
        - **stl**
        - **cecrobodiaxml** (CEC RoboDiA XML environments)
     */
    virtual SceneResourcePtr RegisterScene_UTF8(const std::string& uri, const std::string& scenetype) = 0;

    /// \brief registers scene with default scene type
    virtual SceneResourcePtr RegisterScene_UTF8(const std::string& uri)
    {
        return RegisterScene_UTF8(uri,GetDefaultSceneType());
    }

    /// \see RegisterScene_UTF8
    ///
    /// \param uri utf-16 encoded URI
    virtual SceneResourcePtr RegisterScene_UTF16(const std::wstring& uri, const std::string& scenetype) = 0;

    /// \brief registers scene with default scene type
    virtual SceneResourcePtr RegisterScene_UTF16(const std::wstring& uri)
    {
        return RegisterScene_UTF16(uri,GetDefaultSceneType());
    }

    /** \brief import a scene into COLLADA format using from scene identified by a URI

        \param sourceuri URL-encoded UTF-8  original URI to import from. For MUJIN network files use <b>mujin:/mypath/myfile.ext</b>
        \param sourcescenetype The format of the source file. Can be:
        - **mujincollada**
        - **wincaps** (DensoWave WINCAPS III)
        - **rttoolbox** (Mitsubishi RT ToolBox)
        - **x** (DirectX)
        - **vrml**
        - **stl**
        - **cecrobodiaxml** (CEC RoboDiA XML environments)
        \param newuri UTF-8 encoded new URI to save the imported results. Default is to save to MUJIN COLLADA, so end with <b>.mujin.dae</b> . Use <b>mujin:/mypath/myfile.mujin.dae</b>
        \param overwrite if true, will overwrite any existing scenes at newuri with the new scene.
     */
    virtual SceneResourcePtr ImportSceneToCOLLADA_UTF8(const std::string& sourceuri, const std::string& sourcescenetype, const std::string& newuri, bool overwrite=false) = 0;

    /// \see ImportSceneToCOLLADA_UTF8
    ///
    /// \param sourceuri utf-16 encoded
    /// \param newuri utf-16 encoded
    virtual SceneResourcePtr ImportSceneToCOLLADA_UTF16(const std::wstring& sourceuri, const std::string& sourcescenetype, const std::wstring& newuri, bool overwrite=false) = 0;

    /** \brief Recommended way of uploading a scene's files into the network filesystem.

        Depending on the scenetype, can upload entire directory trees.
        \param sourcefilename UTF-8 encoded local filesystem location of the top-level file. If the scenetype requires many files, will upload all of them. For Windows systems, the \ path separator has to be used. For Unix systems, the / path separator has to be used.
        \param destinationdir UTF-8 encoded destination folder in the network file system. Should always have trailing slash. By default prefix with "mujin:/". Use the / separator for different paths.
        \param scenetype UTF-8 encoded type of scene uploading.
        \throw mujin_exception if the upload fails, will throw an exception
     */
    virtual void SyncUpload_UTF8(const std::string& sourcefilename, const std::string& destinationdir, const std::string& scenetype) = 0;

    /// \see SyncUpload_UTF8
    virtual void SyncUpload_UTF8(const std::string& sourcefilename, const std::string& destinationdir)
    {
        SyncUpload_UTF8(sourcefilename, destinationdir, GetDefaultSceneType());
    }

    /// \see SyncUpload_UTF8
    ///
    /// \param sourcefilename UTF-16 encoded. For Windows systems, the \ path separator has to be used. For Unix systems, the / path separator has to be used.
    /// \param destinationdir UTF-16 encoded
    /// \param scenetype UTF-16 encoded
    virtual void SyncUpload_UTF16(const std::wstring& sourcefilename, const std::wstring& destinationdir, const std::string& scenetype) = 0;

    /// \see SyncUpload
    virtual void SyncUpload_UTF16(const std::wstring& sourcefilename, const std::wstring& destinationdir)
    {
        SyncUpload_UTF16(sourcefilename, destinationdir, GetDefaultSceneType());
    }

    /// \brief Uploads a single file to the controller network filesystem.
    ///
    /// Overwrites the file if it already exists
    /// \param filename utf-8 encoded path of the file on the system
    /// \param desturi UTF-8 encoded destination file in the network filesystem. By default prefix with "mujin:/". Use the / separator for different paths.
    virtual void UploadFileToController_UTF8(const std::string& filename, const std::string& desturi) = 0;

    /// \brief \see UploadFileToController_UTF8
    ///
    /// \param filename utf-16 encoded
    /// \param desturi UTF-16 encoded
    virtual void UploadFileToController_UTF16(const std::wstring& filename, const std::wstring& desturi) = 0;

    /// \brief Uploads binary data to a single file on the controller network filesystem.
    ///
    /// Overwrites the destination uri if it already exists
    /// \param data binary data to upload to the uri
    /// \param desturi UTF-8 encoded destination file in the network filesystem. By default prefix with "mujin:/". Use the / separator for different paths.
    virtual void UploadDataToController_UTF8(const std::vector<unsigned char>& vdata, const std::string& desturi) = 0;

    /** \brief Recursively uploads a directory to the controller network filesystem.

        Creates directories along the way if they don't exist.
        By default, overwrites all the files
        \param copydir is utf-8 encoded. Cannot have trailing slashes '/', '\'
        \param desturi UTF-8 encoded destination file in the network filesystem. If it has a trailing slash, then copydir is inside that URL. If there is no trailing slash, the copydir directory is renamed to the URI. By default prefix with "mujin:/". Use the / separator for different paths.
     */
    virtual void UploadDirectoryToController_UTF8(const std::string& copydir, const std::string& desturi) = 0;

    /// \brief \see UploadDirectoryToController_UTF8
    ///
    /// \param copydir is utf-16 encoded
    /// \param desturi UTF-16 encoded
    virtual void UploadDirectoryToController_UTF16(const std::wstring& copydir, const std::wstring& desturi) = 0;

    /// \param vdata filled with the contents of the file on the controller filesystem
    virtual void DownloadFileFromController_UTF8(const std::string& desturi, std::vector<unsigned char>& vdata) = 0;
    /// \param vdata filled with the contents of the file on the controller filesystem
    virtual void DownloadFileFromController_UTF16(const std::wstring& desturi, std::vector<unsigned char>& vdata) = 0;

    /// \brief Deletes a file on the controller network filesystem.
    ///
    /// \param uri UTF-8 encoded file in the network filesystem to delete.
    virtual void DeleteFileOnController_UTF8(const std::string& uri) = 0;

    /// \brief \see DeleteFileOnController_UTF8
    ///
    /// \param uri UTF-16 encoded file in the network filesystem to delete.
    virtual void DeleteFileOnController_UTF16(const std::wstring& uri) = 0;

    /// \brief Recursively deletes a directory on the controller network filesystem.
    ///
    /// \param uri UTF-8 encoded file in the network filesystem to delete.
    virtual void DeleteDirectoryOnController_UTF8(const std::string& uri) = 0;

    /// \brief \see DeleteDirectoryOnController_UTF8
    ///
    /// \param uri UTF-16 encoded
    virtual void DeleteDirectoryOnController_UTF16(const std::wstring& uri) = 0;

    virtual void SetDefaultSceneType(const std::string& scenetype) = 0;

    virtual const std::string& GetDefaultSceneType() = 0;

    virtual void SetDefaultTaskType(const std::string& tasktype) = 0;

    virtual const std::string& GetDefaultTaskType() = 0;

    /** \brief Get the url-encoded primary key of a scene from a scene uri (utf-8 encoded)

        For example, the URI

        mujin:/検証動作_121122.mujin.dae

        is represented as:

        "mujin:/\xe6\xa4\x9c\xe8\xa8\xbc\xe5\x8b\x95\xe4\xbd\x9c_121122.mujin.dae"

        Return value will be: "%E6%A4%9C%E8%A8%BC%E5%8B%95%E4%BD%9C_121122"
        \param uri utf-8 encoded URI
     */
    virtual std::string GetScenePrimaryKeyFromURI_UTF8(const std::string& uri) = 0;

    /** \brief Get the url-encoded primary key of a scene from a scene uri (utf-16 encoded)

        If input URL is L"mujin:/\u691c\u8a3c\u52d5\u4f5c_121122.mujin.dae"
        Return value will be: "%E6%A4%9C%E8%A8%BC%E5%8B%95%E4%BD%9C_121122"

        \param uri utf-16 encoded URI
     */
    virtual std::string GetScenePrimaryKeyFromURI_UTF16(const std::wstring& uri) = 0;

    /// \brief returns the primary key of a name
    ///
    /// \param name utf-8 encoded
    virtual std::string GetPrimaryKeyFromName_UTF8(const std::string& name) = 0;

    /// \brief returns the primary key of a name
    ///
    /// \param name utf-16 encoded
    virtual std::string GetPrimaryKeyFromName_UTF16(const std::wstring& name) = 0;

    /// \brief returns the uncoded name from a primary key
    ///
    /// \return utf-8 encoded name
    virtual std::string GetNameFromPrimaryKey_UTF8(const std::string& pk) = 0;

    /// \brief returns the uncoded name from a primary key
    ///
    /// \return utf-16 encoded name
    virtual std::wstring GetNameFromPrimaryKey_UTF16(const std::string& pk) = 0;
};

class MUJINCLIENT_API WebResource
{
public:
    WebResource(ControllerClientPtr controller, const std::string& resourcename, const std::string& pk);
    virtual ~WebResource() {
    }

    inline ControllerClientPtr GetController() const {
        return __controller;
    }
    inline const std::string& GetResourceName() const {
        return __resourcename;
    }
    inline const std::string& GetPrimaryKey() const {
        return __pk;
    }

    /// \brief gets an attribute of this web resource
    virtual std::string Get(const std::string& field);

    /// \brief sets an attribute of this web resource
    virtual void Set(const std::string& field, const std::string& newvalue);

    /// \brief delete the resource and all its child resources
    virtual void Delete();

    /// \brief copy the resource and all its child resources to a new name
    virtual void Copy(const std::string& newname, int options);

private:
    ControllerClientPtr __controller;
    std::string __resourcename, __pk;
};

class MUJINCLIENT_API ObjectResource : public WebResource
{
public:
    class MUJINCLIENT_API LinkResource : public WebResource {
public:
        LinkResource(ControllerClientPtr controller, const std::string& objectpk, const std::string& pk);
        virtual ~LinkResource() {
        }

        std::vector<std::string> attachmentpks;
        std::string name;
        std::string pk;
        //TODO transforms
    };
    typedef boost::shared_ptr<LinkResource> LinkResourcePtr;

    ObjectResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~ObjectResource() {
    }

    virtual void GetLinks(std::vector<LinkResourcePtr>& links);

    std::string name;
    int nundof;
    std::string datemodified;
    std::string geometry;
    bool isrobot;
    std::string pk;
    std::string resource_uri;
    std::string scenepk;
    std::string unit;
    std::string uri;

protected:
    ObjectResource(ControllerClientPtr controller, const std::string& resource, const std::string& pk);

};

class MUJINCLIENT_API RobotResource : public ObjectResource
{
public:
    class MUJINCLIENT_API ToolResource : public WebResource {
public:
        ToolResource(ControllerClientPtr controller, const std::string& robotobjectpk, const std::string& pk);
        virtual ~ToolResource() {
        }

        std::string name;
        std::string frame_origin;
        std::string frame_tip;
        std::string pk;
        Real direction[3];
        Real quaternion[4]; // quaternion [w, x, y, z] = [cos(angle/2), sin(angle/2)*rotation_axis]
        Real translate[3];
    };
    typedef boost::shared_ptr<ToolResource> ToolResourcePtr;

    class MUJINCLIENT_API AttachedSensorResource : public WebResource {
public:
        AttachedSensorResource(ControllerClientPtr controller, const std::string& robotobjectpk, const std::string& pk);
        virtual ~AttachedSensorResource() {
        }

        std::string name;
        std::string frame_origin;
        std::string pk;
        //Real direction[3];
        Real quaternion[4]; // quaternion [w, x, y, z] = [cos(angle/2), sin(angle/2)*rotation_axis]
        Real translate[3];
        std::string sensortype;

        class SensorData {
public:
            Real distortion_coeffs[5];
            std::string distortion_model;
            Real focal_length;
            int image_dimensions[3];
            Real intrinsic[6];
            Real measurement_time;
            std::vector<Real> asus_depth_parameters;
        };
        SensorData sensordata;
    };

    typedef boost::shared_ptr<AttachedSensorResource> AttachedSensorResourcePtr;

    RobotResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~RobotResource() {
    }

    virtual void GetTools(std::vector<ToolResourcePtr>& tools);
    virtual void GetAttachedSensors(std::vector<AttachedSensorResourcePtr>& attachedsensors);

    // attachments
    // ikparams
    // images
    int numdof;
    std::string simulation_file;
};

class MUJINCLIENT_API SceneResource : public WebResource
{
public:
    class InstObject;
    typedef boost::shared_ptr<InstObject> InstObjectPtr;
    /// \brief nested resource in the scene describe an object in the scene
    class MUJINCLIENT_API InstObject : public WebResource
    {
public:
        InstObject(ControllerClientPtr controller, const std::string& scenepk, const std::string& pk);
        virtual ~InstObject() {
        }

        class MUJINCLIENT_API Link {
public:
            std::string name;
            Real quaternion[4]; // quaternion [w, x, y, z] = [cos(angle/2), sin(angle/2)*rotation_axis]
            Real translate[3];
        };

        class MUJINCLIENT_API Tool {
public:
            std::string name;
            Real direction[3];
            Real quaternion[4]; // quaternion [w, x, y, z] = [cos(angle/2), sin(angle/2)*rotation_axis]
            Real translate[3];
        };

        class MUJINCLIENT_API Grab {
public:
            std::string instobjectpk; ///< grabed_instobject_pk
            std::string grabbed_linkpk;
            std::string grabbing_linkpk;

            std::string Serialize() {
                return boost::str(boost::format("{\"instobjectpk\": \"%s\", \"grabbed_linkpk\": \"%s\",  \"grabbing_linkpk\": \"%s\"}")%instobjectpk%grabbed_linkpk%grabbing_linkpk);
            }

            bool operator==(const Grab grab) {
                if (this->instobjectpk == grab.instobjectpk
                    && this->grabbed_linkpk == grab.grabbed_linkpk
                    && this->grabbing_linkpk == grab.grabbing_linkpk) {
                    return true;
                }
                return false;
            }
        };

        void SetTransform(const Transform& t);
        void SetDOFValues();
        virtual void GrabObject(InstObjectPtr grabbedobject, std::string& grabbedobjectlinkpk, std::string& grabbinglinkpk);
        virtual void ReleaseObject(InstObjectPtr grabbedobject, std::string& grabbedobjectlinkpk, std::string& grabbinglinkpk);


        std::vector<Real> dofvalues;
        std::string name;
        std::string pk;
        std::string object_pk;
        std::string reference_uri;
        Real quaternion[4]; // quaternion [w, x, y, z] = [cos(angle/2), sin(angle/2)*rotation_axis]
        Real translate[3];
        std::vector<Grab> grabs;
        std::vector<Link> links;
        std::vector<Tool> tools;
    };

    SceneResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~SceneResource() {
    }

    virtual void SetInstObjectsState(const std::vector<SceneResource::InstObjectPtr>& instobjects, const std::vector<InstanceObjectState>& states);

    /** \brief Gets or creates the a task part of the scene

        If task exists already, validates it with tasktype.
        \param taskname utf-8 encoded name of the task to search for or create. If the name already exists and is not tasktype, an exception is thrown
        \param tasktype The type of task to create. Supported types are:
        - itlplanning
     */

    virtual TaskResourcePtr GetOrCreateTaskFromName_UTF8(const std::string& taskname, const std::string& tasktype);

    virtual TaskResourcePtr GetOrCreateTaskFromName_UTF8(const std::string& taskname)
    {
        return GetOrCreateTaskFromName_UTF8(taskname, GetController()->GetDefaultTaskType());
    }

    virtual TaskResourcePtr GetTaskFromName_UTF8(const std::string& taskname);

    /** \brief Gets or creates the a task part of the scene

        If task exists already, validates it with tasktype.
        \param taskname utf-16 encoded name of the task to search for or create. If the name already exists and is not tasktype, an exception is thrown
        \param tasktype The type of task to create. Supported types are:
        - itlplanning
     */
    virtual TaskResourcePtr GetOrCreateTaskFromName_UTF16(const std::wstring& taskname, const std::string& tasktype);

    virtual TaskResourcePtr GetOrCreateTaskFromName_UTF16(const std::wstring& taskname)
    {
        return GetOrCreateTaskFromName_UTF16(taskname, GetController()->GetDefaultTaskType());
    }

    virtual TaskResourcePtr GetTaskFromName_UTF16(const std::wstring& taskname);

    /// \brief gets a list of all the scene primary keys currently available to the user
    virtual void GetTaskPrimaryKeys(std::vector<std::string>& taskkeys);

    /// \brief gets a list of all the instance objects of the scene
    virtual void GetInstObjects(std::vector<InstObjectPtr>& instobjects);
    virtual bool FindInstObject(const std::string& name, InstObjectPtr& instobject);

    virtual SceneResource::InstObjectPtr CreateInstObject(const std::string& name, const std::string& reference_uri, Real quaternion[4], Real translate[3]);

    virtual SceneResourcePtr Copy(const std::string& name);
};

class MUJINCLIENT_API TaskResource : public WebResource
{
public:
    TaskResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~TaskResource() {
    }

    /// \brief execute the task.
    ///
    /// This operation is non-blocking and will return immediately after the execution is started. In order to check if the task is running or is complete, use \ref GetRunTimeStatus() and \ref GetResult()
    /// \return true if task was executed fine
    virtual bool Execute();

    /// \brief if the task is currently executing, send a cancel request
    virtual void Cancel();

    /// \brief get the run-time status of the executed task.
    ///
    /// This will only work if the task has been previously Executed with execute
    /// If the task is not currently running, will set status.code to JSC_Unknown
    /// \param options if options is 1, also get the message
    virtual void GetRunTimeStatus(JobStatus& status, int options = 1);

    /// \brief Gets or creates the a optimization part of the scene
    ///
    /// \param optimizationname the name of the optimization to search for or create
    /// \param optimizaitontype The type of optimization, can be "robotplacement" or "placements"
    virtual OptimizationResourcePtr GetOrCreateOptimizationFromName_UTF8(const std::string& optimizationname, const std::string& optimizationtype=std::string("robotplacement"));

    /// \brief \see GetOrCreateOptimizationFromName_UTF8
    virtual OptimizationResourcePtr GetOrCreateOptimizationFromName_UTF16(const std::wstring& optimizationname, const std::string& optimizationtype=std::string("robotplacement"));

    /// \brief gets a list of all the scene primary keys currently available to the user
    virtual void GetOptimizationPrimaryKeys(std::vector<std::string>& optimizationkeys);

    /// \brief Get the task info for tasks of type <b>itlplanning</b>
    virtual void GetTaskParameters(ITLPlanningTaskParameters& taskparameters);

    /// \brief Set new task info for tasks of type <b>itlplanning</b>
    virtual void SetTaskParameters(const ITLPlanningTaskParameters& taskparameters);

    /// \brief gets the result of the task execution. If no result has been computed yet, will return a NULL pointer.
    virtual PlanningResultResourcePtr GetResult();

protected:
    std::string _jobpk; ///< the job primary key used to track the status of the running task after \ref Execute is called
};

class MUJINCLIENT_API OptimizationResource : public WebResource
{
public:
    OptimizationResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~OptimizationResource() {
    }

    /// \brief execute the optimization
    ///
    /// \param bClearOldResults if true, will clear the old optimiation results. If false, will keep the old optimization results and only compute those that need to be computed.
    virtual void Execute(bool bClearOldResults=true);

    /// \brief if the optimization is currently executing, send a cancel request
    virtual void Cancel();

    /// \brief Set new task info for tasks of type <b>robotplanning</b>
    void SetOptimizationParameters(const RobotPlacementOptimizationParameters& optparams);

    /// \brief Set new task info for tasks of type <b>placements</b>
    void SetOptimizationParameters(const PlacementsOptimizationParameters& optparams);

    /// \brief get the run-time status of the executed optimization.
    ///
    /// This will only work if the optimization has been previously Executed with execute
    /// If the task is not currently running, will set status.code to JSC_Unknown
    /// \param options if options is 1, also get the message
    virtual void GetRunTimeStatus(JobStatus& status, int options = 1);

    /// \brief Gets the results of the optimization execution ordered by task_time.
    ///
    /// \param startoffset The offset to retrieve the results from. Ordered
    /// \param num The number of results to get starting at startoffset. If 0, will return ALL results.
    virtual void GetResults(std::vector<PlanningResultResourcePtr>& results, int startoffset=0, int num=0);

protected:
    std::string _jobpk; ///< the job primary key used to track the status of the running optimization after \ref Execute is called
};

class MUJINCLIENT_API PlanningResultResource : public WebResource
{
public:
    PlanningResultResource(ControllerClientPtr controller, const std::string& resulttype, const std::string& pk);
    PlanningResultResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~PlanningResultResource() {
    }

    /// \brief Get all the transforms the results are storing. Depending on the optimization, can be more than just the robot
    virtual void GetEnvironmentState(EnvironmentState& envstate);

    /** \brief Gets the raw program information

        \param[in] programtype The type of program to return. Possible values are:
        - auto - special type that returns the most suited programs
        - mujinxml - \b xml
        - melfabasicv - \b json with Mitsubishi-specific programs
        - densowaverc8pac - \b json with DensoWave-specific programs
        - cecrobodiasim - zip file

        If \b auto is set, then the robot-maker specific program is returned if possible. If not possible, then mujin xml is returned. All the programs for all robots planned are returned.

        \param[out] outputdata The raw program data
     */
    virtual void GetAllRawProgramData(std::string& outputdata, const std::string& programtype="auto");

    /** \brief Gets the raw program information of a specific robot, if supported.

       \param[in] robotpk The primary key of the robot instance in the scene.
       \param[out] outputdata The raw program data
       \param[in] programtype The type of program to return.
       \throw mujin_exception If robot program is not supported, will throw an exception
     */
    virtual void GetRobotRawProgramData(std::string& outputdata, const std::string& robotpk, const std::string& programtype="auto");

    /// \brief Gets parsed program information
    ///
    /// If the robot doesn't have a recognizable controller, then no programs might be returned.
    /// \param[out] programs The programs for each robot. The best suited program for each robot is determined from its controller.
    /// \param[in] programtype The type of program to return.
    virtual void GetPrograms(RobotControllerPrograms& programs, const std::string& programtype="auto");
};

/** \en \brief creates the controller with an account. <b>This function is not thread safe.</b>

    You must not call it when any other thread in the program (i.e. a thread sharing the same memory) is running.
    \param usernamepassword user:password
    \param url the URI-encoded URL of controller server, it needs to have a trailing slash. It can also be in the form of https://username@server/ in order to force login of a particular user. If not specified, will use the default mujin controller URL
    \param proxyserverport Specify proxy server to use. To specify port number in this string, append :[port] to the end of the host name. The proxy string may be prefixed with [protocol]:// since any such prefix will be ignored. The proxy's port number may optionally be specified with the separate option. If not specified, will default to using port 1080 for proxies. Setting to empty string will disable the proxy.
    \param proxyuserpw If non-empty, [user name]:[password] to use for the connection to the HTTP proxy.
    \param options extra options for connecting to the controller. If 0x1 is set, the client will optimize usage to only allow GET calls. Set 0x80000000 if using a development server.

    \ja \brief MUJINコントローラのクライアントを作成する。<b>この関数はスレッドセーフではない。</b>

    この関数はスレッドセーフではないため、呼び出す時に他のスレッドが走っていないようにご注意ください。
    \param usernamepassword ユーザ:パスワード
    \param url コントローラにアクセスするためのURLです。スラッシュ「/」で終わる必要があります。強制的にユーザも指定出来ます、例えば<b>https://username@server/</b>。指定されていなければデフォールトのMUJINコントローラURLが使用されます。
    \param proxyserverport Specify proxy server to use. To specify port number in this string, append :[port] to the end of the host name. The proxy string may be prefixed with [protocol]:// since any such prefix will be ignored. The proxy's port number may optionally be specified with the separate option. If not specified, will default to using port 1080 for proxies. Setting to empty string will disable the proxy.
    \param proxyuserpw If non-empty, [user name]:[password] to use for the connection to the HTTP proxy.
    \param options １が指定されたら、クライアントがGETのみを呼び出し出来ます。それで初期化がもっと速くなれます。
 */
MUJINCLIENT_API ControllerClientPtr CreateControllerClient(const std::string& usernamepassword, const std::string& url=std::string(), const std::string& proxyserverport=std::string(), const std::string& proxyuserpw=std::string(), int options=0);

/// \brief called at the very end of an application to safely destroy all controller client resources
MUJINCLIENT_API void DestroyControllerClient();

/// \deprecated 14/03/14
MUJINCLIENT_API void ControllerClientDestroy() MUJINCLIENT_DEPRECATED;

/// \brief Compute a 3x4 matrix from a Transform
MUJINCLIENT_API void ComputeMatrixFromTransform(Real matrix[12], const Transform &transform);

/** \brief Compute Euler angles in ZXY order (T = Z*X*Y) from a 3x4 matrix

    Rx = Matrix(3,3,[1,0,0,0,cos(x),-sin(x),0,sin(x),cos(x)])
    Ry = Matrix(3,3,[cos(y),0,sin(y),0,1,0,-sin(y),0,cos(y)])
    Rz = Matrix(3,3,[cos(z),-sin(z),0,sin(z),cos(z),0,0,0,1])
    Rz*Rx*Ry

    [-sin(x)*sin(y)*sin(z) + cos(y)*cos(z), -sin(z)*cos(x),  sin(x)*sin(z)*cos(y) + sin(y)*cos(z)]
    [ sin(x)*sin(y)*cos(z) + sin(z)*cos(y),  cos(x)*cos(z), -sin(x)*cos(y)*cos(z) + sin(y)*sin(z)]
    [                       -sin(y)*cos(x),         sin(x),                         cos(x)*cos(y)]

 */
MUJINCLIENT_API void ComputeZXYFromMatrix(Real ZXY[3], const Real matrix[12]);

MUJINCLIENT_API void ComputeZXYFromTransform(Real ZXY[3], const Transform &transform);

}

#if !defined(MUJINCLIENT_DISABLE_ASSERT_HANDLER) && defined(BOOST_ENABLE_ASSERT_HANDLER)
/// Modifications controlling %boost library behavior.
namespace boost
{
inline void assertion_failed(char const * expr, char const * function, char const * file, long line)
{
    throw mujinclient::MujinException(boost::str(boost::format("[%s:%d] -> %s, expr: %s")%file%line%function%expr),mujinclient::MEC_Assert);
}

#if BOOST_VERSION>104600
inline void assertion_failed_msg(char const * expr, char const * msg, char const * function, char const * file, long line)
{
    throw mujinclient::MujinException(boost::str(boost::format("[%s:%d] -> %s, expr: %s, msg: %s")%file%line%function%expr%msg),mujinclient::MEC_Assert);
}
#endif

}
#endif

BOOST_STATIC_ASSERT(MUJINCLIENT_VERSION_MAJOR>=0&&MUJINCLIENT_VERSION_MAJOR<=255);
BOOST_STATIC_ASSERT(MUJINCLIENT_VERSION_MINOR>=0&&MUJINCLIENT_VERSION_MINOR<=255);
BOOST_STATIC_ASSERT(MUJINCLIENT_VERSION_PATCH>=0&&MUJINCLIENT_VERSION_PATCH<=255);

#endif
