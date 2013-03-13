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
#ifndef MUJINCLIENT_H
#define MUJINCLIENT_H

#ifndef MUJINCLIENT_DISABLE_ASSERT_HANDLER
#define BOOST_ENABLE_ASSERT_HANDLER
#endif

#define BOOST_FILESYSTEM_VERSION 3 // use boost filesystem v3

#ifdef _MSC_VER

#pragma warning(disable:4251) // needs to have dll-interface to be used by clients of class
#pragma warning(disable:4190) // C-linkage specified, but returns UDT 'boost::shared_ptr<T>' which is incompatible with C
#pragma warning(disable:4819) //The file contains a character that cannot be represented in the current code page (932). Save the file in Unicode format to prevent data loss using native typeof

#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCDNAME__
#endif

#else
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
class SceneResource;
class TaskResource;
class OptimizationResource;
class PlanningResultResource;

typedef boost::shared_ptr<ControllerClient> ControllerClientPtr;
typedef boost::weak_ptr<ControllerClient> ControllerClientWeakPtr;
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
        quat[0] = 1; quat[1] = 0; quat[2] = 0; quat[3] = 0;
        translation[0] = 0; translation[1] = 0; translation[2] = 0;
    }
    Real quat[4]; ///< quaternion [cos(ang/2), axis*sin(ang/2)]
    Real translation[3]; ///< translation x,y,z
};

struct SceneInformation
{
    std::string pk; ///< primary key
    std::string uri; ///< uri of the file pointed to
    std::string datemodified; ///< late date modified
    std::string name;
};

/// \brief holds information about the itlplanning task goal
struct ITLPlanningTaskInfo
{
    ITLPlanningTaskInfo() {
        SetDefaults();
    }
    inline void SetDefaults() {
        startfromcurrent = 0;
        returntostart = 1;
        vrcruns = 1;
        unit = "mm";
        outputtrajtype = "robotmaker";
        optimizationvalue = 1;
        program.clear();
    }
    int startfromcurrent; ///< Will start planning from the current robot joint values, otherwise will start at the first waypoint in the program.
    int returntostart; ///< Plan the return path of the robot to where the entire trajectory started. Makes it possible to loop the robot motion.
    int vrcruns; ///< Use the Robot Virtual Controller for retiming and extra validation. Makes planning slow, but robot timing because very accurate.
    std::string unit; ///< the unit that information is used in. m, mm, nm, inch, etc
    std::string outputtrajtype; ///< what format to output the trajectory in.
    Real optimizationvalue; ///< value in [0,1]. 0 is no optimization (fast), 1 is full optimization (slow)
    std::string program; ///< itl program
};

struct RobotPlacementOptimizationInfo
{
    RobotPlacementOptimizationInfo() {
        SetDefaults();
    }
    inline void SetDefaults() {
        topstorecandidates = 20;
        targetname.clear();
        frame = "0 robot";
        unit = "mm";
        minrange[0] = -400; minrange[1] = -400; minrange[2] = 0; minrange[3] = -180;
        maxrange[0] = 400; maxrange[1] = 400; maxrange[2] = 400; maxrange[3] = 90;
        stepsize[0] = 100; stepsize[1] = 100; stepsize[2] = 100; stepsize[3] = 90;
        ignorebasecollision = 1;
    }
    std::string targetname; ///< what target object to optimize for. If blank, will use robot.
    std::string frame; ///< The frame to define the optimization parameters in
    std::string unit; ///< the unit that information is used in. m, mm, nm, inch, etc
    Real maxrange[4]; ///< X, Y, Z, Angle (deg)
    Real minrange[4]; ///< X, Y, Z, Angle (deg)
    Real stepsize[4]; ///< X, Y, Z, Angle (deg)
    int ignorebasecollision; ///< When moving the robot, allow collisions of the base with the environment, this allows users to search for a base placement and while ignoring small obstacles.
    int topstorecandidates; ///< In order to speed things up, store at least the top (fastest) N candidates. Candidates beyond the top N will not be computed.
};

/// \brief Creates on MUJIN Controller instance.
///
/// Only one call can be made at a time. In order to make multiple calls simultaneously, create another instance.
class MUJINCLIENT_API ControllerClient
{
public:
    virtual ~ControllerClient() {
    }

//    /** \brief Returns a list of filenames in the user system of a particular type
//
//        \param scenetype the type of scene possible values are:
//        - mujincollada
//        - wincaps
//        - rttoolbox
//        - cecvirfitxml
//        - stl
//        - x
//        - vrml
//        - stl
//     */
//    virtual void GetSceneFilenames(const std::string& scenetype, std::vector<std::string>& scenefilenames) = 0;

    /// \brief sets the character encoding for all strings that are being input and output from the resources
    ///
    /// The default character encoding is utf-8, can also set it to Shift_JIS for windows japanese unicode, iso-2022-jp
    /// List of possible sets: http://www.iana.org/assignments/character-sets/character-sets.xml
    virtual void SetCharacterEncoding(const std::string& newencoding) = 0;

    /// \brief sets the language code for all output
    ///
    /// Checkout http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
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

        \param format The format of the source file. Can be:
        - **mujincollada**
        - **wincaps** (DensoWave WINCAPS III)
        - **rttoolbox** (Mitsubishi RT ToolBox)
        - **x** (DirectX)
        - **vrml**
        - **stl**
        - **cecvirfitxml** (CEC Virfit XML environments)
     */
    virtual SceneResourcePtr RegisterScene(const std::string& uri, const std::string& format) = 0;

    /// \brief registers scene with default scene type
    virtual SceneResourcePtr RegisterScene(const std::string& uri)
    {
        return RegisterScene(uri,GetDefaultSceneType());
    }

    /** \brief import a scene into COLLADA format using from scene identified by a URI

        \param sourceuri The original URI to import from. For MUJIN network files use <b>mujin:/mypath/myfile.ext</b>
        \param sourceformat The format of the source file. Can be:
        - **mujincollada**
        - **wincaps** (DensoWave WINCAPS III)
        - **rttoolbox** (Mitsubishi RT ToolBox)
        - **x** (DirectX)
        - **vrml**
        - **stl**
        - **cecvirfitxml** (CEC Virfit XML environments)
        \param newuri Then new URI to save the imported results. Default is to save to MUJIN COLLADA, so end with <b>.mujin.dae</b> . Use <b>mujin:/mypath/myfile.mujin.dae</b>
     */
    virtual SceneResourcePtr ImportScene(const std::string& sourceuri, const std::string& sourceformat, const std::string& newuri) = 0;

    /** \brief uploads a particular scene's files into the network filesystem.

        \param sourcefilename Local filesystem location of the top-level file. If the scenetype requires many files, will upload all of them.
        \param destinationdir Destination folder in the network file system. By default use: "mujin:/"
        \param scenetype The type of scene uploading.
     */
    virtual bool SyncUpload(const std::string& sourcefilename, const std::string& destinationdir, const std::string& scenetype) = 0;

    virtual bool SyncUpload(const std::string& sourcefilename, const std::string& destinationdir)
    {
        return SyncUpload(sourcefilename, destinationdir, GetDefaultSceneType());
    }

    virtual void SetDefaultSceneType(const std::string& scenetype) = 0;

    virtual const std::string& GetDefaultSceneType() = 0;

    virtual void SetDefaultTaskType(const std::string& tasktype) = 0;

    virtual const std::string& GetDefaultTaskType() = 0;
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

class MUJINCLIENT_API SceneResource : public WebResource
{
public:
    /// \brief nested resource in the scene describe an object in the scene
    class MUJINCLIENT_API InstObject : public WebResource
    {
public:
        InstObject(ControllerClientPtr controller, const std::string& scenepk, const std::string& pk);
        virtual ~InstObject() {
        }

        std::vector<Real> dofvalues;
        std::string name;
        std::string object_pk;
        std::string reference_uri;
        Real rotate[4]; // quaternion
        Real translate[3];
    };
    typedef boost::shared_ptr<InstObject> InstObjectPtr;

    SceneResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~SceneResource() {
    }

    /** \brief Gets or creates the a task part of the scene

        \param taskname the name of the task to search for or create. If the name already exists and is not tasktype, an exception is thrown
        \param tasktype The type of task to create. Supported types are:
        - itlplanning
     */
    virtual TaskResourcePtr GetOrCreateTaskFromName(const std::string& taskname, const std::string& tasktype);

    virtual TaskResourcePtr GetOrCreateTaskFromName(const std::string& taskname)
    {
        return GetOrCreateTaskFromName(taskname, GetController()->GetDefaultTaskType());
    }


    /// \brief gets a list of all the scene primary keys currently available to the user
    virtual void GetTaskPrimaryKeys(std::vector<std::string>& taskkeys);

    /// \brief gets a list of all the instance objects of the scene
    virtual void GetInstObjects(std::vector<InstObjectPtr>& instobjects);
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

    /// \brief get the run-time status of the executed task.
    ///
    /// This will only work if the task has been previously Executed with execute
    /// If the task is not currently running, will set status.code to JSC_Unknown
    virtual void GetRunTimeStatus(JobStatus& status, int options = 1);

    /// \brief Gets or creates the a optimization part of the scene
    ///
    /// \param optimizationname the name of the optimization to search for or create
    virtual OptimizationResourcePtr GetOrCreateOptimizationFromName(const std::string& optimizationname, const std::string& optimizationtype=std::string("robotplacement"));

    /// \brief gets a list of all the scene primary keys currently available to the user
    virtual void GetOptimizationPrimaryKeys(std::vector<std::string>& optimizationkeys);

    /// \brief Get the task info for tasks of type <b>itlplanning</b>
    virtual void GetTaskInfo(ITLPlanningTaskInfo& taskinfo);

    /// \brief Set new task info for tasks of type <b>itlplanning</b>
    virtual void SetTaskInfo(const ITLPlanningTaskInfo& taskinfo);

    /// \brief gets the result of the task execution. If no result has been computed yet, will return a NULL pointer.
    virtual PlanningResultResourcePtr GetResult();

protected:
    std::string _jobpk;
};

class MUJINCLIENT_API OptimizationResource : public WebResource
{
public:
    OptimizationResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~OptimizationResource() {
    }

    /// \brief execute the optimization
    virtual void Execute();

    /// \brief Set new task info for tasks of type <b>robotplanning</b>
    void SetOptimizationInfo(const RobotPlacementOptimizationInfo& optimizationinfo);

    /// \brief get the run-time status of the executed optimization.
    virtual void GetRunTimeStatus(JobStatus& status);

    /// \brief gets the fastest results of the optimization execution.
    ///
    /// \param fastestnum The number of results to get starting with the fastest task_time. If 0, will return ALL results.
    virtual void GetResults(int fastestnum, std::vector<PlanningResultResourcePtr>& results);
};

class MUJINCLIENT_API PlanningResultResource : public WebResource
{
public:
    PlanningResultResource(ControllerClientPtr controller, const std::string& pk);
    virtual ~PlanningResultResource() {
    }

    /// \brief Get all the transforms the results are storing. Depending on the optimization, can be more than just the robot
    virtual void GetTransforms(std::map<std::string, Transform>& transforms);
};


/** \en \brief creates the controller with an account. <b>This function is not thread safe.</b>

    You must not call it when any other thread in the program (i.e. a thread sharing the same memory) is running.
    \param usernamepassword user:password
    \param url the URL of controller server, it needs to have a trailing slash. It can also be in the form of https://username@server/ in order to force login of a particular user. If not specified, will use the default mujin controller URL
    \param proxyserverport Specify proxy server to use. To specify port number in this string, append :[port] to the end of the host name. The proxy string may be prefixed with [protocol]:// since any such prefix will be ignored. The proxy's port number may optionally be specified with the separate option. If not specified, will default to using port 1080 for proxies. Setting to empty string will disable the proxy.
    \param proxyuserpw If non-empty, [user name]:[password] to use for the connection to the HTTP proxy.
    \param options extra options for connecting to the controller. If 1, the client will optimize usage to only allow GET calls

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
MUJINCLIENT_API void ControllerClientDestroy();

// \brief Get the url-encoded primary key of a scene from a scene uri (utf-8 encoded)
MUJINCLIENT_API std::string GetPrimaryKeyFromURI_UTF8(const std::string& uri);

// \brief Get the url-encoded primary key of a scene from a scene uri (utf-16 encoded)
MUJINCLIENT_API std::string GetPrimaryKeyFromURI_UTF16(const std::wstring& uri);

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
