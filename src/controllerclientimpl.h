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
/** \file controllerclientimpl.h
    \brief  Private header file implementing the ControllerClient interface
 */
#ifndef MUJIN_CONTROLLERCLIENT_IMPL_H
#define MUJIN_CONTROLLERCLIENT_IMPL_H

#include <mujincontrollerclient/mujincontrollerclient.h>

#include <boost/enable_shared_from_this.hpp>
#include <mujincontrollerclient/mujinrapidjsonmsgpack.h>

namespace mujinclient {

class ControllerClientImpl : public ControllerClient, public boost::enable_shared_from_this<ControllerClientImpl>
{
public:
    /// @brief Creates a Mujin controller client instance.
    /// @param internalClient If True, uses internal communication (ZMQ and MsgPack) instead of the public HTTP interface and JSON.
    ControllerClientImpl(const std::string& usernamepassword, const std::string& baseuri, const std::string& proxyserverport, const std::string& proxyuserpw, int options, double timeout, bool isInternalClient=false);
    virtual ~ControllerClientImpl();

    virtual void InitializeZMQ(const int zmqPort, boost::shared_ptr<zmq::context_t> zmqcontext);

    virtual const std::string& GetUserName() const;
    virtual const std::string& GetBaseURI() const;

    virtual std::string GetVersion();
    virtual void SetCharacterEncoding(const std::string& newencoding);
    virtual void SetProxy(const std::string& serverport, const std::string& userpw);
    virtual void SetLanguage(const std::string& language);
    virtual void SetUserAgent(const std::string& userAgent);
    virtual void SetAdditionalHeaders(const std::vector<std::string>& additionalHeaders);
    virtual void RestartServer(double timeout);
    virtual void _ExecuteGraphQuery(const char* operationName, const char* query, const rapidjson::Value& rVariables, rapidjson::Value& rResult, rapidjson::Document::AllocatorType& rAlloc, double timeout, bool checkForErrors, bool returnRawResponse, bool useInternalComm);
    virtual void ExecuteGraphQuery(const char* operationName, const char* query, const rapidjson::Value& rVariables, rapidjson::Value& rResult, rapidjson::Document::AllocatorType& rAlloc, double timeout);
    virtual void ExecuteGraphQueryRaw(const char* operationName, const char* query, const rapidjson::Value& rVariables, rapidjson::Value& rResult, rapidjson::Document::AllocatorType& rAlloc, double timeout);
    virtual void CancelAllJobs();
    virtual void GetRunTimeStatuses(std::vector<JobStatus>& statuses, int options);
    virtual void GetScenePrimaryKeys(std::vector<std::string>& scenekeys);
    virtual SceneResourcePtr RegisterScene_UTF8(const std::string& uri, const std::string& scenetype);
    virtual SceneResourcePtr RegisterScene_UTF16(const std::wstring& uri, const std::string& scenetype);
    virtual SceneResourcePtr ImportSceneToCOLLADA_UTF8(const std::string& importuri, const std::string& importformat, const std::string& newuri, bool overwrite=false);
    virtual SceneResourcePtr ImportSceneToCOLLADA_UTF16(const std::wstring& importuri, const std::string& importformat, const std::wstring& newuri, bool overwrite=false);

    virtual void SyncUpload_UTF8(const std::string& sourcefilename, const std::string& destinationdir, const std::string& scenetype);
    virtual void SyncUpload_UTF16(const std::wstring& sourcefilename_utf16, const std::wstring& destinationdir_utf16, const std::string& scenetype);

    virtual void UploadFileToController_UTF8(const std::string& filename, const std::string& desturi);
    virtual void UploadFileToController_UTF16(const std::wstring& filename, const std::wstring& desturi);
    virtual void UploadDataToController_UTF8(const void* data, size_t size, const std::string& desturi);
    virtual void UploadDataToController_UTF16(const void* data, size_t size, const std::wstring& desturi);
    virtual void UploadDirectoryToController_UTF8(const std::string& copydir, const std::string& desturi);
    virtual void UploadDirectoryToController_UTF16(const std::wstring& copydir, const std::wstring& desturi);
    virtual void DownloadFileFromController_UTF8(const std::string& desturi, std::vector<unsigned char>& vdata);
    virtual void DownloadFileFromController_UTF16(const std::wstring& desturi, std::vector<unsigned char>& vdata);
    virtual void DownloadFileFromControllerIfModifiedSince_UTF8(const std::string& desturi, long localtimeval, long &remotetimeval, std::vector<unsigned char>& vdata, double timeout);
    virtual void DownloadFileFromControllerIfModifiedSince_UTF16(const std::wstring& desturi, long localtimeval, long &remotetimeval, std::vector<unsigned char>& vdata, double timeout);
    virtual long GetModifiedTime(const std::string& uri, double timeout);
    virtual void DeleteFileOnController_UTF8(const std::string& desturi);
    virtual void DeleteFileOnController_UTF16(const std::wstring& desturi);
    virtual void DeleteDirectoryOnController_UTF8(const std::string& desturi);
    virtual void DeleteDirectoryOnController_UTF16(const std::wstring& desturi);
    virtual void ListFilesInController(std::vector<FileEntry>& fileentries, const std::string &dirname, double timeout);

    virtual void SaveBackup(std::ostream& outputStream, bool config, bool media, const std::string& backupscenepks, double timeout);
    virtual void RestoreBackup(std::istream& inputStream, bool config, bool media, double timeout);
    virtual void Upgrade(std::istream& inputStream, bool autorestart, bool uploadonly, double timeout);
    virtual bool GetUpgradeStatus(std::string& status, double &progress, double timeout);
    virtual void CancelUpgrade(double timeout);
    virtual void Reboot(double timeout);
    virtual void DeleteAllScenes(double timeout);
    virtual void DeleteAllITLPrograms(double timeout);

    virtual void ModifySceneAddReferenceObjectPK(const std::string &scenepk, const std::string &referenceobjectpk, double timeout = 5.0);
    virtual void ModifySceneRemoveReferenceObjectPK(const std::string &scenepk, const std::string &referenceobjectpk, double timeout = 5.0);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, rapidjson::Document& pt, int expectedhttpcode=200, double timeout = 5.0);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, std::string& outputdata, int expectedhttpcode=200, double timeout = 5.0);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, std::ostream& outputStream, int expectedhttpcode=200, double timeout = 5.0);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, std::vector<unsigned char>& outputdata, int expectedhttpcode=200, double timeout = 5.0);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    ///
    /// \param relativeuri URL-encoded UTF-8 encoded
    /// \param data encoded depending on the character encoding set on the system
    int CallPost(const std::string& relativeuri, const std::string& data, rapidjson::Document& pt, int expectedhttpcode=201, double timeout = 5.0);

    /// \param data utf-8 encoded
    int CallPost_UTF8(const std::string& relativeuri, const std::string& data, rapidjson::Document& pt, int expectedhttpcode=201, double timeout = 5.0);
    /// \param data utf-16 encoded
    int CallPost_UTF16(const std::string& relativeuri, const std::wstring& data, rapidjson::Document& pt, int expectedhttpcode=201, double timeout = 5.0);

    /// \brief puts json string
    /// \param relativeuri relative uri to put at
    /// \param data json string to put
    /// \param pt reply is stored
    /// \param expectedhttpcode expected http code
    /// \param timeout timeout of puts
    /// \return http code returned
    int CallPutJSON(const std::string& relativeuri, const std::string& data, rapidjson::Document& pt, int expectedhttpcode=202, double timeout = 5.0);

    /// \brief puts stl data
    /// \param relativeuri relative uri to put at
    /// \param data stl raw data
    /// \param pt reply is stored
    /// \param expectedhttpcode expected http code
    /// \param timeout timeout of puts
    /// \return http code returned
    int CallPutSTL(const std::string& relativeuri, const std::vector<unsigned char>& data, rapidjson::Document& pt, int expectedhttpcode=202, double timeout = 5.0);


    void CallDelete(const std::string& relativeuri, int expectedhttpcode, double timeout = 5.0);

    std::stringstream& GetBuffer();

    virtual void SetDefaultSceneType(const std::string& scenetype);
    virtual const std::string& GetDefaultSceneType();
    virtual void SetDefaultTaskType(const std::string& tasktype);
    virtual const std::string& GetDefaultTaskType();

    std::string GetScenePrimaryKeyFromURI_UTF8(const std::string& uri);
    std::string GetScenePrimaryKeyFromURI_UTF16(const std::wstring& uri);
    std::string GetPrimaryKeyFromName_UTF8(const std::string& name);
    std::string GetPrimaryKeyFromName_UTF16(const std::wstring& name);
    std::string GetNameFromPrimaryKey_UTF8(const std::string& pk);
    std::wstring GetNameFromPrimaryKey_UTF16(const std::string& pk);

    /// \brief create geometry for a link of an object
    /// \param objectPk primary key for the object
    /// \param geometryName name of the geometry
    /// \param linkPk primary key for the link
    /// \param timeout timeout of creating object geometry
    /// \return primary key for the geometry created
    std::string CreateObjectGeometry(const std::string& objectPk, const std::string& geometryName, const std::string& linkPk, const std::string& geomtype, double timeout = 5);

    std::string CreateIkParam(const std::string& objectPk, const std::string& name, const std::string& iktype, double timeout = 5);
    std::string CreateLink(const std::string& objectPk, const std::string& parentlinkPk, const std::string& name, const Real quaternion[4], const Real translate[3], double timeout = 5);

    /// \brief set geometry for an object
    /// \param objectPk primary key for the object
    /// \param scenePk primary key for the scene
    /// \param meshData mesh data to be set to the object
    /// \param timeout timeout of creating object geometry
    /// \return primary key for the geometry created
    std::string SetObjectGeometryMesh(const std::string& objectPk, const std::string& scenePk, const std::vector<unsigned char>& meshData, const std::string& unit = "mm", double timeout = 5);

    void GetDebugInfos(std::vector<DebugResourcePtr>& debuginfos, double timeout = 5);

    inline std::string GetBaseUri() const
    {
        return _baseuri;
    }

    /// \brief URL-encode raw string
    inline std::string EscapeString(const std::string& raw) const
    {
        boost::shared_ptr<char> pstr(curl_easy_escape(_curl, raw.c_str(), raw.size()), curl_free);
        return std::string(pstr.get());
    }

    /// \brief decode URL-encoded raw string
    inline std::string UnescapeString(const std::string& raw) const
    {
        int outlength = 0;
        boost::shared_ptr<char> pstr(curl_easy_unescape(_curl, raw.c_str(), raw.size(), &outlength), curl_free);
        return std::string(pstr.get(), outlength);
    }


protected:

    int _CallPut(const std::string& relativeuri, const void* pdata, size_t nDataSize, rapidjson::Document& pt, curl_slist* headers, int expectedhttpcode=202, double timeout = 5.0);

    static int _WriteStringStreamCallback(char *data, size_t size, size_t nmemb, std::stringstream *writerData);
    static int _WriteVectorCallback(char *data, size_t size, size_t nmemb, std::vector<unsigned char> *writerData);
    static int _WriteOStreamCallback(char *data, size_t size, size_t nmemb, std::ostream *writerData);
    static int _ReadIStreamCallback(char *data, size_t size, size_t nmemb, std::istream *writerData);

    /// \brief sets up http header for doing http operation with json data
    void _SetupHTTPHeadersJSON();

    /// \brief sets up http header for doing http operation with stl data
    void _SetupHTTPHeadersSTL();

    /// \brief sets up http header for doing http operation with multipart/form-data data
    void _SetupHTTPHeadersMultipartFormData();

    /// \brief given a raw uri with "mujin:/", return the real network uri
    ///
    /// mutex should be locked
    /// \param bEnsurePath if true, will make sure the directories on the server side are created
    /// \param bEnsureSlash if true, will ensure returned uri ends with slash /
    std::string _PrepareDestinationURI_UTF8(const std::string& rawuri, bool bEnsurePath=true, bool bEnsureSlash=false, bool bIsDirectory=false);
    std::string _PrepareDestinationURI_UTF16(const std::wstring& rawuri, bool bEnsurePath=true, bool bEnsureSlash=false, bool bIsDirectory=false);

    // encode a URL without the / separator
    std::string _EncodeWithoutSeparator(const std::string& raw);

    /// \param relativeuri utf-8 encoded directory inside the user webdav folder. has a trailing slash. relative to real uri
    void _EnsureWebDAVDirectories(const std::string& relativeuri, double timeout = 3.0);

    /// For all webdav internal functions: mutex is already locked, desturi directories are already created
    //@{

    /// \param desturi expects the fully resolved URI to pass to curl
    int _CallGet(const std::string& desturi, rapidjson::Document& pt, int expectedhttpcode=200, double timeout = 5.0);
    int _CallGet(const std::string& desturi, std::string& outputdata, int expectedhttpcode=200, double timeout = 5.0);
    int _CallGet(const std::string& desturi, std::vector<unsigned char>& outputdata, int expectedhttpcode=200, double timeout = 5.0);
    int _CallGet(const std::string& desturi, std::ostream& outputStream, int expectedhttpcode=200, double timeout = 5.0);
    int _CallPost(const std::string& desturi, const std::string& data, rapidjson::Document& pt, int expectedhttpcode=201, double timeout = 5.0);

    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _UploadFileToController_UTF8(const std::string& filename, const std::string& desturi);
    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _UploadFileToController_UTF16(const std::wstring& filename, const std::string& desturi);

    /// \brief uploads a single file, to dest location specified by filename
    ///
    /// overwrites the file if it already exists.
    /// \param inputStream the stream represententing the backup. It needs to be seekable to get the size (ifstream subclass is applicable to files).
    virtual void _UploadFileToControllerViaForm(std::istream& inputStream, const std::string& filename, const std::string& endpoint, double timeout = 0);

    /// \brief uploads a single file, to dest location specified by filename
    ///
    /// overwrites the file if it already exists.
    /// \param data the buffer represententing the file
    /// \param size the length of the buffer represententing the file
    virtual void _UploadDataToControllerViaForm(const void* data, size_t size, const std::string& filename, const std::string& endpoint, double timeout = 0);

    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _UploadDirectoryToController_UTF8(const std::string& copydir, const std::string& desturi);
    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _UploadDirectoryToController_UTF16(const std::wstring& wcopydir, const std::string& desturi);

    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _DeleteFileOnController(const std::string& desturi);
    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _DeleteDirectoryOnController(const std::string& desturi);

    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _DownloadFileFromController(const std::string& desturi, long localtimeval, long &remotetimeval, std::vector<unsigned char>& vdata, double timeout = 5);

    //@}

    /// \brief read upload function for win32.
    /// MUST also provide this read callback using CURLOPT_READFUNCTION. Failing to do so will give you a crash since a DLL may not use the variable's memory when passed in to it from an app like this. */
    static size_t _ReadUploadCallback(void *ptr, size_t size, size_t nmemb, void *stream);

    /// \param stream is std::pair<std::vector<unsigned char>::const_iterator, size_t>*, which gets incremented everytime this function is called.
    static size_t _ReadInMemoryUploadCallback(void *ptr, size_t size, size_t nmemb, void *stream);

    int _lastmode;
    CURL *_curl;
    boost::mutex _mutex;
    std::stringstream _buffer;
    std::string _baseuri, _baseapiuri, _basewebdavuri, _uri, _username;

    curl_slist *_httpheadersjson;
    curl_slist *_httpheadersstl;
    curl_slist *_httpheadersmultipartformdata;
    std::string _charset, _language;
    std::string _csrfmiddlewaretoken;
    std::vector<std::string> _additionalHeaders; ///< list of "Header-Name: header-value" additional http headers

    rapidjson::Document _profile; ///< user profile and versioning
    std::string _errormessage; ///< set when an error occurs in libcurl

    std::string _defaultscenetype, _defaulttasktype;

    rapidjson::StringBuffer _rRequestStringBufferCache; ///< cache for request string, protected by _mutex

    bool _isInternalClient;  // If True, uses ZMQ and MsgPack for communication with the controller. If False, uses HTTP and JSON.
    boost::shared_ptr<zmq::context_t> _zmqcontext;
    std::string _mujinControllerIp;
    int _zmqPort;
    ZmqMujinControllerClientPtr _zmqMujinGraphQLClient; ///< a zmq client to send GraphQL queries
    bool _zmqIsInitialized;
};

typedef boost::shared_ptr<ControllerClientImpl> ControllerClientImplPtr;

} // end namespace mujinclient

#endif
