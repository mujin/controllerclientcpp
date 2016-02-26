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

namespace mujinclient {

class ControllerClientImpl : public ControllerClient, public boost::enable_shared_from_this<ControllerClientImpl>
{
public:
    ControllerClientImpl(const std::string& usernamepassword, const std::string& baseuri, const std::string& proxyserverport, const std::string& proxyuserpw, int options);
    virtual ~ControllerClientImpl();

    virtual const std::string& GetUserName() const;
    
    virtual std::string GetVersion();
    virtual void SetCharacterEncoding(const std::string& newencoding);
    virtual void SetProxy(const std::string& serverport, const std::string& userpw);
    virtual void SetLanguage(const std::string& language);
    virtual void RestartServer();
    virtual void Upgrade(const std::vector<unsigned char>& vdata);
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
    virtual void UploadDataToController_UTF8(const std::vector<unsigned char>& vdata, const std::string& desturi);
    virtual void UploadDataToController_UTF16(const std::vector<unsigned char>& vdata, const std::wstring& desturi);
    virtual void UploadDirectoryToController_UTF8(const std::string& copydir, const std::string& desturi);
    virtual void UploadDirectoryToController_UTF16(const std::wstring& copydir, const std::wstring& desturi);
    virtual void DownloadFileFromController_UTF8(const std::string& desturi, std::vector<unsigned char>& vdata);
    virtual void DownloadFileFromController_UTF16(const std::wstring& desturi, std::vector<unsigned char>& vdata);
    virtual void DownloadFileFromControllerIfModifiedSince_UTF8(const std::string& desturi, long localtimeval, long &remotetimeval, std::vector<unsigned char>& vdata);
    virtual void DownloadFileFromControllerIfModifiedSince_UTF16(const std::wstring& desturi, long localtimeval, long &remotetimeval, std::vector<unsigned char>& vdata);
    virtual void DeleteFileOnController_UTF8(const std::string& desturi);
    virtual void DeleteFileOnController_UTF16(const std::wstring& desturi);
    virtual void DeleteDirectoryOnController_UTF8(const std::string& desturi);
    virtual void DeleteDirectoryOnController_UTF16(const std::wstring& desturi);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, boost::property_tree::ptree& pt, int expectedhttpcode=200);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, std::string& outputdata, int expectedhttpcode=200);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, std::vector<unsigned char>& outputdata, int expectedhttpcode=200);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    ///
    /// \param relativeuri URL-encoded UTF-8 encoded
    /// \param data encoded depending on the character encoding set on the system
    int CallPost(const std::string& relativeuri, const std::string& data, boost::property_tree::ptree& pt, int expectedhttpcode=201);

    /// \param data utf-8 encoded
    int CallPost_UTF8(const std::string& relativeuri, const std::string& data, boost::property_tree::ptree& pt, int expectedhttpcode=201);
    /// \param data utf-16 encoded
    int CallPost_UTF16(const std::string& relativeuri, const std::wstring& data, boost::property_tree::ptree& pt, int expectedhttpcode=201);

    int CallPut(const std::string& relativeuri, const std::string& data, boost::property_tree::ptree& pt, int expectedhttpcode=202);

    void CallDelete(const std::string& relativeuri);

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

    inline CURL* GetCURL() const
    {
        return _curl;
    }

    inline boost::shared_ptr<char> GetURLEscapedString(const std::string& name) const
    {
        return boost::shared_ptr<char>(curl_easy_escape(_curl, name.c_str(), name.size()), curl_free);
    }

    inline std::string GetBaseUri() const
    {
        return _baseuri;
    }

protected:

    void GetProfile();

    static int _WriteStringStreamCallback(char *data, size_t size, size_t nmemb, std::stringstream *writerData);
    static int _WriteVectorCallback(char *data, size_t size, size_t nmemb, std::vector<unsigned char> *writerData);

    void _SetHTTPHeaders();

    std::string _GetCSRFFromCookies();

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
    void _EnsureWebDAVDirectories(const std::string& relativeuri);

    /// For all webdav internal functions: mutex is already locked, desturi directories are already created
    //@{

    /// \param desturi expects the fully resolved URI to pass to curl
    int _CallGet(const std::string& desturi, boost::property_tree::ptree& pt, int expectedhttpcode=200);
    int _CallGet(const std::string& desturi, std::string& outputdata, int expectedhttpcode=200);
    int _CallGet(const std::string& desturi, std::vector<unsigned char>& outputdata, int expectedhttpcode=200);

    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _UploadFileToController_UTF8(const std::string& filename, const std::string& desturi);
    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _UploadFileToController_UTF16(const std::wstring& filename, const std::string& desturi);

    /// \brief uploads a single file, assumes the directory already exists
    ///
    /// overwrites the file if it already exists.
    /// \param fd FILE pointer of binary reading file. does not close the handle
    virtual void _UploadFileToController(FILE* fd, const std::string& uri);

    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _UploadDataToController(const std::vector<unsigned char>& vdata, const std::string& desturi);

    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _UploadDirectoryToController_UTF8(const std::string& copydir, const std::string& desturi);
    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _UploadDirectoryToController_UTF16(const std::wstring& wcopydir, const std::string& desturi);

    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _DeleteFileOnController(const std::string& desturi);
    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _DeleteDirectoryOnController(const std::string& desturi);

    /// \brief desturi is URL-encoded. Also assume _mutex is locked.
    virtual void _DownloadFileFromController(const std::string& desturi, long localtimeval, long &remotetimeval, std::vector<unsigned char>& vdata);

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

    curl_slist *_httpheaders;
    std::string _charset, _language;
    std::string _csrfmiddlewaretoken;

    boost::property_tree::ptree _profile; ///< user profile and versioning
    std::string _errormessage; ///< set when an error occurs in libcurl

    std::string _defaultscenetype, _defaulttasktype;
};

typedef boost::shared_ptr<ControllerClientImpl> ControllerClientImplPtr;

} // end namespace mujinclient

#endif
