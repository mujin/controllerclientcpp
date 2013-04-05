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

    virtual std::string GetVersion();
    virtual void SetCharacterEncoding(const std::string& newencoding);
    virtual void SetProxy(const std::string& serverport, const std::string& userpw);
    virtual void SetLanguage(const std::string& language);
    virtual void RestartServer();
    virtual void Upgrade(const std::vector<unsigned char>& vdata);
    virtual void CancelAllJobs();
    virtual void GetRunTimeStatuses(std::vector<JobStatus>& statuses, int options);
    virtual void GetScenePrimaryKeys(std::vector<std::string>& scenekeys);
    virtual SceneResourcePtr RegisterScene(const std::string& uri, const std::string& scenetype);
    virtual SceneResourcePtr ImportSceneToCOLLADA(const std::string& importuri, const std::string& importformat, const std::string& newuri);

    virtual void SyncUpload_UTF8(const std::string& sourcefilename, const std::string& destinationdir, const std::string& scenetype);

    virtual void SyncUpload_UTF16(const std::wstring& sourcefilename_utf16, const std::wstring& destinationdir_utf16, const std::string& scenetype);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, boost::property_tree::ptree& pt, int expectedhttpcode=200);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, std::string& outputdata, int expectedhttpcode=200);

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallPost(const std::string& relativeuri, const std::string& data, boost::property_tree::ptree& pt, int expectedhttpcode=201);

    int CallPut(const std::string& relativeuri, const std::string& data, boost::property_tree::ptree& pt, int expectedhttpcode=202);

    void CallDelete(const std::string& relativeuri);

    std::stringstream& GetBuffer();

    virtual void SetDefaultSceneType(const std::string& scenetype);
    virtual const std::string& GetDefaultSceneType();
    virtual void SetDefaultTaskType(const std::string& tasktype);
    virtual const std::string& GetDefaultTaskType();

    std::string GetScenePrimaryKeyFromURI_UTF8(const std::string& uri);

    std::string GetScenePrimaryKeyFromURI_UTF16(const std::wstring& uri);

protected:

    void GetProfile();

    static int _writer(char *data, size_t size, size_t nmemb, std::stringstream *writerData);

    void _SetHTTPHeaders();

    std::string _GetCSRFFromCookies();

    // encode a URL without the / separator
    std::string _EncodeWithoutSeparator(const std::string& raw);

    /// \param destinationdir the directory inside the user webdav folder. has a trailing slash
    void _EnsureWebDAVDirectories(const std::string& uriDestinationDir);

    /// \brief recursively uploads a directory and creates directories along the way if they don't exist
    ///
    /// overwrites all the files
    /// \param copydir is utf-8 encoded
    /// \param uri is URI-encoded
    void _UploadDirectoryToWebDAV_UTF8(const std::string& copydir, const std::string& uri);

    /// \brief recursively uploads a directory and creates directories along the way if they don't exist
    ///
    /// overwrites all the files
    /// \param copydir is utf-16 encoded
    /// \param uri is URI-encoded
    void _UploadDirectoryToWebDAV_UTF16(const std::wstring& copydir, const std::string& uri);

    /// \brief uploads a single file, assumes the directory already exists
    ///
    /// overwrites the file if it already exists
    /// \param filename utf-8 encoded
    void _UploadFileToWebDAV_UTF8(const std::string& filename, const std::string& uri);

    void _UploadFileToWebDAV_UTF16(const std::wstring& filename, const std::string& uri);

    /// \brief uploads a single file, assumes the directory already exists
    ///
    /// overwrites the file if it already exists.
    /// \param fd FILE pointer of binary reading file. does not close the handle
    void _UploadFileToWebDAV(FILE* fd, const std::string& uri);

    /// \brief read upload function for win32.
    /// MUST also provide this read callback using CURLOPT_READFUNCTION. Failing to do so will give you a crash since a DLL may not use the variable's memory when passed in to it from an app like this. */
    static size_t _ReadUploadCallback(void *ptr, size_t size, size_t nmemb, void *stream);

    int _lastmode;
    CURL *_curl;
    boost::mutex _mutex;
    std::stringstream _buffer;
    std::string _baseuri, _baseapiuri, _basewebdavuri, _uri;

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
