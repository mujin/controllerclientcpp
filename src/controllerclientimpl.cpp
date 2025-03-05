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
#include "common.h"
#include "controllerclientimpl.h"

#include <boost/algorithm/string.hpp>
#include <boost/scope_exit.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <strstream>

#define SKIP_PEER_VERIFICATION // temporary
//#define SKIP_HOSTNAME_VERIFICATION

#include "logging.h"

MUJIN_LOGGER("mujin.controllerclientcpp");

#define CURL_OPTION_SAVER(curl, curlopt, curvalue) boost::shared_ptr<void> __curloptionsaver ## curlopt((void*)0, boost::bind(boost::function<CURLcode(CURL*, CURLoption, decltype(curvalue))>(curl_easy_setopt), curl, curlopt, curvalue))
#define CURL_OPTION_SETTER(curl, curlopt, newvalue) CHECKCURLCODE(curl_easy_setopt(curl, curlopt, newvalue), "curl_easy_setopt " # curlopt)
#define CURL_OPTION_SAVE_SETTER(curl, curlopt, curvalue, newvalue) CURL_OPTION_SAVER(curl, curlopt, curvalue); CURL_OPTION_SETTER(curl, curlopt, newvalue)
#define CURL_INFO_GETTER(curl, curlinfo, outvalue) CHECKCURLCODE(curl_easy_getinfo(curl, curlinfo, outvalue), "curl_easy_getinfo " # curlinfo)
#define CURL_PERFORM(curl) CHECKCURLCODE(curl_easy_perform(curl), "curl_easy_perform")

struct CURLFormReleaser {
    struct curl_httppost *& form;

    ~CURLFormReleaser()
    {
        curl_formfree(std::exchange(form, nullptr));
    }
};

namespace mujinclient {

using namespace mujinjson;

/// \brief given a port string "80", fill ControllerClientInfo httpPort
static void _ParseClientInfoPort(const char* port, size_t length, ControllerClientInfo& clientInfo)
{
    clientInfo.httpPort = 0;
    for (; length > 0; ++port, --length) {
        clientInfo.httpPort = clientInfo.httpPort * 10 + (*port - '0');
    }
}

/// \brief given a url "http[s]://[username[:password]@]hostname[:port][/path]", parse ControllerClientInfo
static void _ParseClientInfoFromURL(const char* url, ControllerClientInfo& clientInfo)
{
    clientInfo.Reset();
    const char* colonSlashSlash = strstr(url, "://");
    if (colonSlashSlash == nullptr) {
        return;
    }
    const char* hostname = colonSlashSlash + sizeof("://") - 1;
    const char* at = strstr(hostname, "@"); // not found is ok
    const char* slash = strstr(hostname, "/"); // not found is ok
    if (at != nullptr && (slash == nullptr || at < slash)) {
        // if the at is before the slash, i.e. for the username:password
        const char* usernamePassword = hostname;
        hostname = at + sizeof("@") - 1;
        const char* colon = strstr(usernamePassword, ":"); // not found is ok
        if (colon != nullptr) {
            const char* password = colon + sizeof(":") - 1;
            clientInfo.username = std::string(usernamePassword, colon - usernamePassword);
            clientInfo.password = std::string(password, at - password);
        } else {
            clientInfo.username = std::string(usernamePassword, at - usernamePassword);
        }
    }
    const char* port = strstr(hostname, ":"); // not found is ok
    if (slash == nullptr) {
        if (port == nullptr) {
            // no port, no slash
            clientInfo.host = hostname;
        } else {
            // has port, no slash
            const char* portStart = port + sizeof(":") - 1;
            _ParseClientInfoPort(portStart, strlen(portStart), clientInfo);
            clientInfo.host = std::string(hostname, port - hostname);
        }
    } else {
        if (port != nullptr && port < slash) {
            // has port before slash
            const char* portStart = port + sizeof(":") - 1;
            _ParseClientInfoPort(portStart, slash - portStart, clientInfo);
            clientInfo.host = std::string(hostname, port - hostname);
        } else {
            // no port, but has slash
            clientInfo.host = std::string(hostname, slash - hostname);
        }
    }
}

template <typename T>
std::wstring ParseWincapsWCNPath(const T& sourcefilename, const boost::function<std::string(const T&)>& ConvertToFileSystemEncoding)
{
    // scenefilenames is the WPJ file, so have to open it up to see what directory it points to
    // note that the encoding is utf-16
    // <clsProject Object="True">
    //   <WCNPath VT="8">.\threegoaltouch\threegoaltouch.WCN;</WCNPath>
    // </clsProject>
    // first have to get the raw utf-16 data
#if defined(_WIN32) || defined(_WIN64)
    std::ifstream wpjfilestream(sourcefilename.c_str(), std::ios::binary|std::ios::in);
#else
    // linux doesn't mix ifstream and wstring
    std::ifstream wpjfilestream(ConvertToFileSystemEncoding(sourcefilename).c_str(), std::ios::binary|std::ios::in);
#endif
    if( !wpjfilestream ) {
        throw MUJIN_EXCEPTION_FORMAT("failed to open file %s", ConvertToFileSystemEncoding(sourcefilename), MEC_InvalidArguments);
    }
    std::wstringstream utf16stream;
    bool readbom = false;
    while(!wpjfilestream.eof() ) {
        unsigned short c;
        wpjfilestream.read(reinterpret_cast<char*>(&c),sizeof(c));
        if( !wpjfilestream ) {
            break;
        }
        // skip the first character (BOM) due to a bug in boost property_tree (should be fixed in 1.49)
        if( readbom || c != 0xfeff ) {
            utf16stream << static_cast<wchar_t>(c);
        }
        else {
            readbom = true;
        }
    }
    boost::property_tree::wptree wpj;
    boost::property_tree::read_xml(utf16stream, wpj);
    boost::property_tree::wptree& clsProject = wpj.get_child(L"clsProject");
    boost::property_tree::wptree& WCNPath = clsProject.get_child(L"WCNPath");
    std::wstring strWCNPath = WCNPath.data();
    if( strWCNPath.size() > 0 ) {
        // post process the string to get the real filesystem directory
        if( strWCNPath.at(strWCNPath.size()-1) == L';') {
            strWCNPath.resize(strWCNPath.size()-1);
        }

        if( strWCNPath.size() >= 2 && (strWCNPath[0] == L'.' && strWCNPath[1] == L'\\') ) {
            // don't need the prefix
            strWCNPath = strWCNPath.substr(2);
        }
    }

    return strWCNPath;
}

ControllerClientImpl::ControllerClientImpl(const std::string& usernamepassword, const std::string& baseuri, const std::string& proxyserverport, const std::string& proxyuserpw, int options, double timeout)
{
    BOOST_ASSERT( !baseuri.empty() );
    const size_t usernameindex = usernamepassword.find_first_of(':');
    BOOST_ASSERT(usernameindex != std::string::npos );
    _username = usernamepassword.substr(0, usernameindex);
    const std::string password = usernamepassword.substr(usernameindex + 1);

    _httpheadersjson = NULL;
    _httpheadersstl = NULL;
    _httpheadersmultipartformdata = NULL;

    size_t authorityindex = baseuri.find("//");
    if( authorityindex != std::string::npos ) {
        _fulluri = baseuri.substr(0,authorityindex+2) + usernamepassword + "@" + baseuri.substr(authorityindex+2);
    }
    else {
        // no idea what to do here..
        _fulluri = std::string("//") + usernamepassword + "@" + baseuri;
    }

    _ParseClientInfoFromURL(baseuri.c_str(), _clientInfo);
    _clientInfo.username = _username;
    _clientInfo.password = password;

    _baseuri = baseuri;
    // ensure trailing slash
    if( _baseuri[_baseuri.size()-1] != '/' ) {
        _baseuri.push_back('/');
    }
    _baseapiuri = _baseuri + std::string("api/v1/");
    // hack for now since webdav server and api server could be running on different ports
    if( boost::algorithm::ends_with(_baseuri, ":8000/") || (options&0x80000000) ) {
        // testing on localhost, however the webdav server is running on port 80...
        _basewebdavuri = str(boost::format("%s/u/%s/")%_baseuri.substr(0,_baseuri.size()-6)%_username);
    }
    else {
        _basewebdavuri = str(boost::format("%su/%s/")%_baseuri%_username);
    }

    //CURLcode code = curl_global_init(CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32);
    _curl = curl_easy_init();
    BOOST_ASSERT(!!_curl);

#ifdef _DEBUG
    // CURL_OPTION_SETTER(_curl, CURLOPT_VERBOSE, 1L);
#endif
    _errormessage.resize(CURL_ERROR_SIZE);
    CURL_OPTION_SETTER(_curl, CURLOPT_ERRORBUFFER, &_errormessage[0]);

#ifdef SKIP_PEER_VERIFICATION
    /*
     * if you want to connect to a site who isn't using a certificate that is
     * signed by one of the certs in the ca bundle you have, you can skip the
     * verification of the server's certificate. this makes the connection
     * a lot less secure.
     *
     * if you have a ca cert for the server stored someplace else than in the
     * default bundle, then the curlopt_capath option might come handy for
     * you.
     */
    CURL_OPTION_SETTER(_curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

#ifdef SKIP_HOSTNAME_VERIFICATION
    /*
     * If the site you're connecting to uses a different host name that what
     * they have mentioned in their server certificate's commonName (or
     * subjectAltName) fields, libcurl will refuse to connect. You can skip
     * this check, but this will make the connection less secure.
     */
    CURL_OPTION_SETTER(_curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

    if( proxyserverport.size() > 0 ) {
        SetProxy(proxyserverport, proxyuserpw);
    }

    CURL_OPTION_SETTER(_curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    CURL_OPTION_SETTER(_curl, CURLOPT_USERPWD, usernamepassword.c_str());

    // need to set the following?
    //CURLOPT_USERAGENT
    //CURLOPT_TCP_KEEPIDLE
    //CURLOPT_TCP_KEEPALIVE
    //CURLOPT_TCP_KEEPINTVL

    CURL_OPTION_SETTER(_curl, CURLOPT_COOKIEFILE, ""); // just to start the cookie engine

    // save everything to _buffer, neceesary to do it before first POST/GET calls or data will be output to stdout
    // these should be set on individual calls
    // CURL_OPTION_SETTER(_curl, CURLOPT_WRITEFUNCTION, _WriteStringStreamCallback); // just to start the cookie engine
    // CURL_OPTION_SETTER(_curl, CURLOPT_WRITEDATA, &_buffer);

    std::string useragent = std::string("controllerclientcpp/")+MUJINCLIENT_VERSION_STRING;
    CURL_OPTION_SETTER(_curl, CURLOPT_USERAGENT, useragent.c_str());

    CURL_OPTION_SETTER(_curl, CURLOPT_FOLLOWLOCATION, 1L); // we can always follow redirect now, we don't need to detect login page
    CURL_OPTION_SETTER(_curl, CURLOPT_MAXREDIRS, 10L);
    CURL_OPTION_SETTER(_curl, CURLOPT_NOSIGNAL, 1L);

    CURL_OPTION_SETTER(_curl, CURLOPT_POSTFIELDSIZE, 0L);
    CURL_OPTION_SETTER(_curl, CURLOPT_POSTFIELDS, NULL);

    // csrftoken can be any non-empty string
    _csrfmiddlewaretoken = "csrftoken";
    std::string cookie = "Set-Cookie: csrftoken=" + _csrfmiddlewaretoken;
#if CURL_AT_LEAST_VERSION(7,60,0)
    // with https://github.com/curl/curl/commit/b8d5036ec9b702d6392c97a6fc2e141d6c7cce1f, setting domain param to cookie is required.
    if(_baseuri.find('/') == _baseuri.size()-1) {
        // _baseuri should be hostname with trailing slash
        cookie += "; domain=";
        cookie += _baseuri.substr(0,_baseuri.size()-1);
    } else {
        CURLU *url = curl_url();
        BOOST_SCOPE_EXIT_ALL(&url) {
            curl_url_cleanup(url);
        };
        CHECKCURLUCODE(curl_url_set(url, CURLUPART_URL, _baseuri.c_str(), 0), "cannot parse url");
        char *host = NULL;
        BOOST_SCOPE_EXIT_ALL(&host) {
            if(host) {
                curl_free(host);
            }
        };
        CHECKCURLUCODE(curl_url_get(url, CURLUPART_HOST, &host, 0), "cannot determine hostname from url");
        cookie += "; domain=";
        cookie += host;
    }
#endif
    CURL_OPTION_SETTER(_curl, CURLOPT_COOKIELIST, cookie.c_str());

    _charset = "utf-8";
    _language = "en-us";
#if defined(_WIN32) || defined(_WIN64)
    UINT codepage = GetACP();
    std::map<int, std::string>::const_iterator itcodepage = encoding::GetCodePageMap().find(codepage);
    if( itcodepage != encoding::GetCodePageMap().end() ) {
        _charset = itcodepage->second;
    }
#endif
    MUJIN_LOG_VERBOSE("setting character set to " << _charset);
    _SetupHTTPHeadersJSON();
    _SetupHTTPHeadersSTL();
    _SetupHTTPHeadersMultipartFormData();
}

ControllerClientImpl::~ControllerClientImpl()
{
    if( !!_httpheadersjson ) {
        curl_slist_free_all(_httpheadersjson);
    }
    if( !!_httpheadersstl ) {
        curl_slist_free_all(_httpheadersstl);
    }
    if( !!_httpheadersmultipartformdata ) {
        curl_slist_free_all(_httpheadersmultipartformdata);
    }
    curl_easy_cleanup(_curl);
}

std::string ControllerClientImpl::GetVersion()
{
    if (!_profile.IsObject()) {
        _profile.SetObject();
        CallGet("profile/", _profile);
    }
    return GetJsonValueByKey<std::string>(_profile, "version");
}

const std::string& ControllerClientImpl::GetUserName() const
{
    return _username;
}

const std::string& ControllerClientImpl::GetBaseURI() const
{
    return _baseuri;
}

const ControllerClientInfo& ControllerClientImpl::GetClientInfo() const
{
    return _clientInfo;
}

void ControllerClientImpl::SetCharacterEncoding(const std::string& newencoding)
{
    boost::mutex::scoped_lock lock(_mutex);
    _charset = newencoding;
    _SetupHTTPHeadersJSON();
    // the following two format does not need charset
    // _SetupHTTPHeadersSTL();
    // _SetupHTTPHeadersMultipartFormData();
}

void ControllerClientImpl::SetProxy(const std::string& serverport, const std::string& userpw)
{
    // mutally exclusive with unix endpoint settings
    CURL_OPTION_SETTER(_curl, CURLOPT_UNIX_SOCKET_PATH, NULL);
    CURL_OPTION_SETTER(_curl, CURLOPT_PROXY, serverport.c_str());
    CURL_OPTION_SETTER(_curl, CURLOPT_PROXYUSERPWD, userpw.c_str());
    _clientInfo.unixEndpoint.clear();
}

void ControllerClientImpl::SetUnixEndpoint(const std::string& unixendpoint)
{
    // mutually exclusive with proxy settings
    CURL_OPTION_SETTER(_curl, CURLOPT_PROXY, NULL);
    CURL_OPTION_SETTER(_curl, CURLOPT_PROXYUSERPWD, NULL);
    CURL_OPTION_SETTER(_curl, CURLOPT_UNIX_SOCKET_PATH, unixendpoint.c_str());
    _clientInfo.unixEndpoint = unixendpoint;
}

void ControllerClientImpl::SetLanguage(const std::string& language)
{
    boost::mutex::scoped_lock lock(_mutex);
    if (language!= "") {
        _language = language;
    }
    _SetupHTTPHeadersJSON();
    // the following two format does not need language
    // _SetupHTTPHeadersSTL();
    // _SetupHTTPHeadersMultipartFormData();
}

void ControllerClientImpl::SetUserAgent(const std::string& userAgent)
{
    CURL_OPTION_SETTER(_curl, CURLOPT_USERAGENT, userAgent.c_str());
}

void ControllerClientImpl::SetAdditionalHeaders(const std::vector<std::string>& additionalHeaders)
{
    boost::mutex::scoped_lock lock(_mutex);
    _additionalHeaders = additionalHeaders;
    _clientInfo.additionalHeaders = additionalHeaders;
    _SetupHTTPHeadersJSON();
    _SetupHTTPHeadersSTL();
    _SetupHTTPHeadersMultipartFormData();
}

void ControllerClientImpl::_ExecuteGraphQuery(const char* operationName, const char* query, const rapidjson::Value& rVariables, rapidjson::Value& rResult, rapidjson::Document::AllocatorType& rAlloc, double timeout, bool checkForErrors, bool returnRawResponse)
{
    rResult.SetNull(); // zero output

    rapidjson::Value rResultDoc;

    {
        boost::mutex::scoped_lock lock(_mutex);

        rapidjson::StringBuffer& rRequestStringBuffer = _rRequestStringBufferCache;
        rRequestStringBuffer.Clear();

        {
            // use the callers allocator to construct the request body
            rapidjson::Value rRequest, rValue;
            rRequest.SetObject();
            rValue.SetString(operationName, rAlloc);
            rRequest.AddMember(rapidjson::Document::StringRefType("operationName"), rValue, rAlloc);
            rValue.SetString(query, rAlloc);
            rRequest.AddMember(rapidjson::Document::StringRefType("query"), rValue, rAlloc);
            rValue.CopyFrom(rVariables, rAlloc);
            rRequest.AddMember(rapidjson::Document::StringRefType("variables"), rValue, rAlloc);

            rapidjson::Writer<rapidjson::StringBuffer> writer(rRequestStringBuffer);
            rRequest.Accept(writer);
        }

        _uri = _baseuri + "api/v2/graphql";
        _CallPost(_uri, rRequestStringBuffer.GetString(), rResultDoc, rAlloc, 200, timeout);
    }

    // parse response
    if (!rResultDoc.IsObject()) {
        throw MUJIN_EXCEPTION_FORMAT("Execute graph query does not return valid response \"%s\", invalid response: %s", operationName%mujinjson::DumpJson(rResultDoc), MEC_HTTPServer);
    }

    if (checkForErrors) {
        // look for errors in response
        const rapidjson::Value::ConstMemberIterator itErrors = rResultDoc.FindMember("errors");
        if (itErrors != rResultDoc.MemberEnd() && itErrors->value.IsArray() && itErrors->value.Size() > 0) {
            MUJIN_LOG_VERBOSE(str(boost::format("graph query has errors \"%s\": %s")%operationName%mujinjson::DumpJson(rResultDoc)));
            for (rapidjson::Value::ConstValueIterator itError = itErrors->value.Begin(); itError != itErrors->value.End(); ++itError) {
                const rapidjson::Value& rError = *itError;
                if (rError.IsObject() && rError.HasMember("message") && rError["message"].IsString()) {
                    const char* errorCode = "unknown";
                    const rapidjson::Value::ConstMemberIterator itExtensions = rError.FindMember("extensions");
                    if (itExtensions != rError.MemberEnd() && itExtensions->value.IsObject() && itExtensions->value.HasMember("errorCode") && itExtensions->value["errorCode"].IsString()) {
                        errorCode = itExtensions->value["errorCode"].GetString();
                    }
                    throw mujinclient::MujinGraphQueryError(boost::str(boost::format("[%s:%d] graph query has errors \"%s\": %s")%(__PRETTY_FUNCTION__)%(__LINE__)%operationName%rError["message"].GetString()), errorCode);
                }
            }
            throw MUJIN_EXCEPTION_FORMAT("graph query has undefined errors \"%s\": %s", operationName%mujinjson::DumpJson(rResultDoc), MEC_HTTPServer);
        }
    }

    // should have data member
    if (!rResultDoc.HasMember("data")) {
        throw MUJIN_EXCEPTION_FORMAT("Execute graph query does not have 'data' field in \"%s\", invalid response: %s", operationName%mujinjson::DumpJson(rResultDoc), MEC_HTTPServer);
    }

    // set output
    if (returnRawResponse) {
        rResult.Swap(rResultDoc);
    } else {
        rResult = rResultDoc["data"];
    }
}

void ControllerClientImpl::ExecuteGraphQuery(const char* operationName, const char* query, const rapidjson::Value& rVariables, rapidjson::Value& rResult, rapidjson::Document::AllocatorType& rAlloc, double timeout)
{
    _ExecuteGraphQuery(operationName, query, rVariables, rResult, rAlloc, timeout, true, false);
}

void ControllerClientImpl::ExecuteGraphQueryRaw(const char* operationName, const char* query, const rapidjson::Value& rVariables, rapidjson::Value& rResult, rapidjson::Document::AllocatorType& rAlloc, double timeout)
{
    _ExecuteGraphQuery(operationName, query, rVariables, rResult, rAlloc, timeout, false, true);
}

GraphSubscriptionHandlerPtr ControllerClientImpl::ExecuteGraphSubscription(const std::string& operationName, const std::string& query, const rapidjson::Value& rVariables, std::function<void(const boost::system::error_code&, rapidjson::Value&&)> onReadHandler)
{
    boost::mutex::scoped_lock lock(_mutex);
    GraphSubscriptionWebSocketHandlerPtr graphSubscriptionWebSocketHandler = _graphSubscriptionWebSocketHandler.lock();
    if (!graphSubscriptionWebSocketHandler || !graphSubscriptionWebSocketHandler->IsStreamOpen()) {
        // create a websocket connection
        graphSubscriptionWebSocketHandler = boost::make_shared<GraphSubscriptionWebSocketHandler>(_clientInfo);
    }

    std::string subscriptionId = graphSubscriptionWebSocketHandler->StartSubscription(operationName, query, rVariables, onReadHandler);
    _graphSubscriptionWebSocketHandler = GraphSubscriptionWebSocketHandlerWeakPtr(graphSubscriptionWebSocketHandler);

    return boost::make_shared<GraphSubscriptionHandlerImpl>(graphSubscriptionWebSocketHandler, subscriptionId);
}

void ControllerClientImpl::RestartServer(double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, (long)(timeout * 1000L));
    _uri = _baseuri + std::string("restartserver/");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, _uri.c_str());
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_POST, 0L, 1L);
    _buffer.clear();
    _buffer.str("");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &_buffer);
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( http_code != 200 ) {
        throw MUJIN_EXCEPTION_FORMAT0("Failed to restart server, please try again or contact MUJIN support", MEC_HTTPServer);
    }
}

void ControllerClientImpl::CancelAllJobs()
{
    CallDelete("job/?format=json", 204);
}

void ControllerClientImpl::GetRunTimeStatuses(std::vector<JobStatus>& statuses, int options)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    std::string url = "job/?format=json&fields=pk,status,fnname,elapsedtime";
    if( options & 1 ) {
        url += std::string(",status_text");
    }
    CallGet(url, pt);
    rapidjson::Value& objects = pt["objects"];
    size_t i = 0;
    statuses.resize(objects.Size());
    for (rapidjson::Document::ValueIterator it=objects.Begin(); it != objects.End(); ++it) {

        statuses[i].pk = GetJsonValueByKey<std::string>(*it, "pk");
        statuses[i].code = GetStatusCode(GetJsonValueByKey<std::string>(*it, "status"));
        statuses[i].type = GetJsonValueByKey<std::string>(*it, "fnname");
        statuses[i].elapsedtime = GetJsonValueByKey<double>(*it, "elapsedtime");
        if( options & 1 ) {
            statuses[i].message = GetJsonValueByKey<std::string>(*it, "status_text");
        }
        i++;
    }
}

void ControllerClientImpl::GetScenePrimaryKeys(std::vector<std::string>& scenekeys)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    CallGet("scene/?format=json&limit=0&fields=pk", pt);
    rapidjson::Value& objects = pt["objects"];
    scenekeys.resize(objects.Size());
    size_t i = 0;
    for (rapidjson::Document::ValueIterator it=objects.Begin(); it != objects.End(); ++it) {
        scenekeys[i++] = GetJsonValueByKey<std::string>(*it, "pk");
    }
}

SceneResourcePtr ControllerClientImpl::RegisterScene_UTF8(const std::string& uri, const std::string& scenetype)
{
    BOOST_ASSERT(scenetype.size()>0);
    rapidjson::Document pt(rapidjson::kObjectType);
    CallPost_UTF8("scene/?format=json&fields=pk", str(boost::format("{\"uri\":\"%s\", \"scenetype\":\"%s\"}")%uri%scenetype), pt);
    std::string pk = GetJsonValueByKey<std::string>(pt, "pk");
    SceneResourcePtr scene(new SceneResource(shared_from_this(), pk));
    return scene;
}

SceneResourcePtr ControllerClientImpl::RegisterScene_UTF16(const std::wstring& uri, const std::string& scenetype)
{
    BOOST_ASSERT(scenetype.size()>0);
    rapidjson::Document pt(rapidjson::kObjectType);
    CallPost_UTF16("scene/?format=json&fields=pk", str(boost::wformat(L"{\"uri\":\"%s\", \"scenetype\":\"%s\"}")%uri%scenetype.c_str()), pt);
    std::string pk = GetJsonValueByKey<std::string>(pt, "pk");
    SceneResourcePtr scene(new SceneResource(shared_from_this(), pk));
    return scene;
}

SceneResourcePtr ControllerClientImpl::ImportSceneToCOLLADA_UTF8(const std::string& importuri, const std::string& importformat, const std::string& newuri, bool overwrite)
{
    BOOST_ASSERT(importformat.size()>0);
    rapidjson::Document pt(rapidjson::kObjectType);
    CallPost_UTF8(str(boost::format("scene/?format=json&fields=pk&overwrite=%d")%overwrite), str(boost::format("{\"reference_uri\":\"%s\", \"reference_scenetype\":\"%s\", \"uri\":\"%s\"}")%importuri%importformat%newuri), pt);
    std::string pk = GetJsonValueByKey<std::string>(pt, "pk");
    SceneResourcePtr scene(new SceneResource(shared_from_this(), pk));
    return scene;
}

SceneResourcePtr ControllerClientImpl::ImportSceneToCOLLADA_UTF16(const std::wstring& importuri, const std::string& importformat, const std::wstring& newuri, bool overwrite)
{
    BOOST_ASSERT(importformat.size()>0);
    rapidjson::Document pt(rapidjson::kObjectType);
    CallPost_UTF16(str(boost::format("scene/?format=json&fields=pk&overwrite=%d")%overwrite), str(boost::wformat(L"{\"reference_uri\":\"%s\", \"reference_scenetype\":\"%s\", \"uri\":\"%s\"}")%importuri%importformat.c_str()%newuri), pt);
    std::string pk = GetJsonValueByKey<std::string>(pt, "pk");
    SceneResourcePtr scene(new SceneResource(shared_from_this(), pk));
    return scene;
}

void ControllerClientImpl::SyncUpload_UTF8(const std::string& _sourcefilename, const std::string& destinationdir, const std::string& scenetype)
{
    // TODO use curl_multi_perform to allow uploading of multiple files simultaneously
    // TODO should LOCK with WebDAV repository?
    boost::mutex::scoped_lock lock(_mutex);
    std::string baseuploaduri;
    if( destinationdir.size() >= 7 && destinationdir.substr(0,7) == "mujin:/" ) {
        baseuploaduri = _basewebdavuri;
        baseuploaduri += _EncodeWithoutSeparator(destinationdir.substr(7));
        _EnsureWebDAVDirectories(destinationdir.substr(7));
    }
    else {
        baseuploaduri = destinationdir;
    }
    // ensure trailing slash
    if( baseuploaduri[baseuploaduri.size()-1] != '/' ) {
        baseuploaduri.push_back('/');
    }

    std::string sourcefilename = _sourcefilename;
#if defined(_WIN32) || defined(_WIN64)
    // check if / is used anywhere and send a warning if it is
    if( sourcefilename.find_first_of('/') != std::string::npos ) {
        std::stringstream ss;
        ss << "scene filename '" << sourcefilename << "' is using /, so replacing this with \\";
        MUJIN_LOG_INFO(ss.str());
        for(size_t i = 0; i < sourcefilename.size(); ++i) {
            if( sourcefilename[i] == '/' ) {
                sourcefilename[i] = '\\';
            }
        }
    }
#endif

    size_t nBaseFilenameStartIndex = sourcefilename.find_last_of(s_filesep);
    if( nBaseFilenameStartIndex == std::string::npos ) {
        // there's no path?
        nBaseFilenameStartIndex = 0;
    }
    else {
        nBaseFilenameStartIndex++;
    }

    if( scenetype == "wincaps" ) {
        std::wstring strWCNPath_utf16 = ParseWincapsWCNPath<std::string>(sourcefilename, encoding::ConvertUTF8ToFileSystemEncoding);
        if( strWCNPath_utf16.size() > 0 ) {
            std::string strWCNPath;
            utf8::utf16to8(strWCNPath_utf16.begin(), strWCNPath_utf16.end(), std::back_inserter(strWCNPath));
            std::string strWCNURI = strWCNPath;
            size_t lastindex = 0;
            for(size_t i = 0; i < strWCNURI.size(); ++i) {
                if( strWCNURI[i] == '\\' ) {
                    strWCNURI[i] = '/';
                    strWCNPath[i] = s_filesep;
                    lastindex = i;
                }
            }
            std::string sCopyDir = sourcefilename.substr(0,nBaseFilenameStartIndex) + strWCNPath.substr(0,lastindex);
            _UploadDirectoryToController_UTF8(sCopyDir, baseuploaduri+_EncodeWithoutSeparator(strWCNURI.substr(0,lastindex)));
        }
    }
    else if( scenetype == "rttoolbox" || scenetype == "cecrobodiaxml" ) {
        if( nBaseFilenameStartIndex > 0 ) {
            // sourcefilename[nBaseFilenameStartIndex] should be == s_filesep
            _UploadDirectoryToController_UTF8(sourcefilename.substr(0,nBaseFilenameStartIndex), baseuploaduri);
        }
        return;
    }

    // sourcefilenamebase is utf-8
    std::string uploadfileuri = baseuploaduri + EscapeString(sourcefilename.substr(nBaseFilenameStartIndex));
    _UploadFileToController_UTF8(sourcefilename, uploadfileuri);

    /* webdav operations
       const char *postdata =
       "<?xml version=\"1.0\"?><D:searchrequest xmlns:D=\"DAV:\" >"
       "<D:sql>SELECT \"http://schemas.microsoft.com/repl/contenttag\""
       " from SCOPE ('deep traversal of \"/exchange/adb/Calendar/\"') "
       "WHERE \"DAV:isfolder\" = True</D:sql></D:searchrequest>\r\n";
     */
}

void ControllerClientImpl::SyncUpload_UTF16(const std::wstring& _sourcefilename_utf16, const std::wstring& destinationdir_utf16, const std::string& scenetype)
{
    // TODO use curl_multi_perform to allow uploading of multiple files simultaneously
    // TODO should LOCK with WebDAV repository?
    boost::mutex::scoped_lock lock(_mutex);
    std::string baseuploaduri;
    std::string destinationdir_utf8;
    utf8::utf16to8(destinationdir_utf16.begin(), destinationdir_utf16.end(), std::back_inserter(destinationdir_utf8));

    if( destinationdir_utf8.size() >= 7 && destinationdir_utf8.substr(0,7) == "mujin:/" ) {
        baseuploaduri = _basewebdavuri;
        std::string s = destinationdir_utf8.substr(7);
        baseuploaduri += _EncodeWithoutSeparator(s);
        _EnsureWebDAVDirectories(s);
    }
    else {
        baseuploaduri = destinationdir_utf8;
    }
    // ensure trailing slash
    if( baseuploaduri[baseuploaduri.size()-1] != '/' ) {
        baseuploaduri.push_back('/');
    }

    std::wstring sourcefilename_utf16 = _sourcefilename_utf16;
#if defined(_WIN32) || defined(_WIN64)
    // check if / is used anywhere and send a warning if it is
    if( sourcefilename_utf16.find_first_of(L'/') != std::wstring::npos ) {
        std::stringstream ss;
        ss << "scene filename '" << encoding::ConvertUTF16ToFileSystemEncoding(sourcefilename_utf16) << "' is using /, so replacing this with \\";
        MUJIN_LOG_INFO(ss.str());
        for(size_t i = 0; i < sourcefilename_utf16.size(); ++i) {
            if( sourcefilename_utf16[i] == L'/' ) {
                sourcefilename_utf16[i] = L'\\';
            }
        }

        boost::replace_all(sourcefilename_utf16, L"/", L"\\");
    }
#endif

    size_t nBaseFilenameStartIndex = sourcefilename_utf16.find_last_of(s_wfilesep);
    if( nBaseFilenameStartIndex == std::string::npos ) {
        // there's no path?
        nBaseFilenameStartIndex = 0;
    }
    else {
        nBaseFilenameStartIndex++;
    }

    if( scenetype == "wincaps" ) {
        std::wstring strWCNPath_utf16 = ParseWincapsWCNPath<std::wstring>(sourcefilename_utf16, encoding::ConvertUTF16ToFileSystemEncoding);
        if( strWCNPath_utf16.size() > 0 ) {
            std::string strWCNURI;
            utf8::utf16to8(strWCNPath_utf16.begin(), strWCNPath_utf16.end(), std::back_inserter(strWCNURI));
            size_t lastindex_utf8 = 0;
            for(size_t i = 0; i < strWCNURI.size(); ++i) {
                if( strWCNURI[i] == '\\' ) {
                    strWCNURI[i] = '/';
                    lastindex_utf8 = i;
                }
            }
            size_t lastindex_utf16 = 0;
            for(size_t i = 0; i < strWCNPath_utf16.size(); ++i) {
                if( strWCNPath_utf16[i] == '\\' ) {
                    strWCNPath_utf16[i] = s_wfilesep;
                    lastindex_utf16 = i;
                }
            }
            std::wstring sCopyDir_utf16 = sourcefilename_utf16.substr(0,nBaseFilenameStartIndex) + strWCNPath_utf16.substr(0,lastindex_utf16);
            _UploadDirectoryToController_UTF16(sCopyDir_utf16, baseuploaduri+_EncodeWithoutSeparator(strWCNURI.substr(0,lastindex_utf8)));
        }
    }
    else if( scenetype == "rttoolbox" || scenetype == "cecrobodiaxml" ) {
        if( nBaseFilenameStartIndex > 0 ) {
            // sourcefilename_utf16[nBaseFilenameStartIndex] should be == s_filesep
            _UploadDirectoryToController_UTF16(sourcefilename_utf16.substr(0,nBaseFilenameStartIndex), baseuploaduri);
        }
        return;
    }

    // sourcefilenamebase is utf-8
    std::string sourcefilenamedir_utf8;
    utf8::utf16to8(sourcefilename_utf16.begin()+nBaseFilenameStartIndex, sourcefilename_utf16.end(), std::back_inserter(sourcefilenamedir_utf8));
    std::string uploadfileuri = baseuploaduri + EscapeString(sourcefilenamedir_utf8);
    _UploadFileToController_UTF16(sourcefilename_utf16, uploadfileuri);
}

/// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
int ControllerClientImpl::CallGet(const std::string& relativeuri, rapidjson::Document& pt, int expectedhttpcode, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    _uri = _baseapiuri;
    _uri += relativeuri;
    return _CallGet(_uri, pt, pt.GetAllocator(), expectedhttpcode, timeout);
}

int ControllerClientImpl::_CallGet(const std::string& desturi, rapidjson::Value& rResponse, rapidjson::Document::AllocatorType& alloc, int expectedhttpcode, double timeout)
{
    MUJIN_LOG_DEBUG(str(boost::format("GET %s")%desturi));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, (long)(timeout * 1000L));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, desturi.c_str());
    _buffer.clear();
    _buffer.str("");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &_buffer);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPGET, 0L, 1L);
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( _buffer.rdbuf()->in_avail() > 0 ) {
        mujinjson::ParseJson(rResponse, alloc, _buffer);
    }
    if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
        std::string error_message = GetJsonValueByKey<std::string>(rResponse, "error_message");
        throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s: %s", desturi%http_code%error_message, MEC_HTTPServer);
    }
    return http_code;
}

int ControllerClientImpl::CallGet(const std::string& relativeuri, std::string& outputdata, int expectedhttpcode, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    _uri = _baseapiuri;
    _uri += relativeuri;
    return _CallGet(_uri, outputdata, expectedhttpcode, timeout);
}

int ControllerClientImpl::_CallGet(const std::string& desturi, std::string& outputdata, int expectedhttpcode, double timeout)
{
    MUJIN_LOG_VERBOSE(str(boost::format("GET %s")%desturi));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, (long)(timeout * 1000L));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, desturi.c_str());
    _buffer.clear();
    _buffer.str("");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &_buffer);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPGET, 0L, 1L);
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    outputdata = _buffer.str();
    if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
        if( outputdata.size() > 0 ) {
            rapidjson::Document d;
            ParseJson(d, _buffer.str());
            std::string error_message = GetJsonValueByKey<std::string>(d, "error_message");
            throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s: %s", desturi%http_code%error_message, MEC_HTTPServer);
        }
        throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s", desturi%http_code, MEC_HTTPServer);
    }
    return http_code;
}

int ControllerClientImpl::CallGet(const std::string& relativeuri, std::ostream& outputStream, int expectedhttpcode, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    _uri = _baseapiuri;
    _uri += relativeuri;
    return _CallGet(_uri, outputStream, expectedhttpcode, timeout);
}

int ControllerClientImpl::_CallGet(const std::string& desturi, std::ostream& outputStream, int expectedhttpcode, double timeout)
{
    MUJIN_LOG_VERBOSE(str(boost::format("GET %s")%desturi));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, (long)(timeout * 1000L));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, desturi.c_str());
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteOStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &outputStream);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPGET, 0L, 1L);
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
        // outputStream is not always seekable; ignore any error message.
        throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s (outputStream might have information)", desturi%http_code, MEC_HTTPServer);
    }
    return http_code;
}

int ControllerClientImpl::CallGet(const std::string& relativeuri, std::vector<unsigned char>& outputdata, int expectedhttpcode, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    _uri = _baseapiuri;
    _uri += relativeuri;
    return _CallGet(_uri, outputdata, expectedhttpcode, timeout);
}

int ControllerClientImpl::_CallGet(const std::string& desturi, std::vector<unsigned char>& outputdata, int expectedhttpcode, double timeout)
{
    MUJIN_LOG_VERBOSE(str(boost::format("GET %s")%desturi));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, (long)(timeout * 1000L));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, desturi.c_str());

    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteVectorCallback);
    outputdata.resize(0);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &outputdata);

    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPGET, 0L, 1L);
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
        if( outputdata.size() > 0 ) {
            rapidjson::Document d;
            std::stringstream ss;
            ss.write((const char*)&outputdata[0], outputdata.size());
            ParseJson(d, ss.str());
            std::string error_message = GetJsonValueByKey<std::string>(d, "error_message");
            throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s: %s", desturi%http_code%error_message, MEC_HTTPServer);
        }
        throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s", desturi%http_code, MEC_HTTPServer);
    }
    return http_code;
}

/// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
int ControllerClientImpl::CallPost(const std::string& relativeuri, const std::string& data, rapidjson::Document& pt, int expectedhttpcode, double timeout)
{
    MUJIN_LOG_DEBUG(str(boost::format("POST %s%s")%_baseapiuri%relativeuri));
    boost::mutex::scoped_lock lock(_mutex);
    _uri = _baseapiuri;
    _uri += relativeuri;
    return _CallPost(_uri, data, pt, pt.GetAllocator(), expectedhttpcode, timeout);
}

/// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
int ControllerClientImpl::_CallPost(const std::string& desturi, const std::string& data, rapidjson::Value& rResult, rapidjson::Document::AllocatorType& alloc, int expectedhttpcode, double timeout)
{
    MUJIN_LOG_VERBOSE(str(boost::format("POST(json) %s")%desturi));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, (long)(timeout * 1000L));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, desturi.c_str());
    _buffer.clear();
    _buffer.str("");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &_buffer);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_POST, 0L, 1L);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_POSTFIELDSIZE, 0, data.size());
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_POSTFIELDS, NULL, data.size() > 0 ? data.c_str() : NULL);
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( _buffer.rdbuf()->in_avail() > 0 ) {
        ParseJson(rResult, alloc, _buffer);
    } else {
        rResult.SetObject();
    }
    if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
        std::string error_message = GetJsonValueByKey<std::string>(rResult, "error_message");
        throw MUJIN_EXCEPTION_FORMAT("HTTP POST to '%s' returned HTTP status %s: %s", desturi%http_code%error_message, MEC_HTTPServer);
    }
    return http_code;
}

int ControllerClientImpl::_CallPost(const std::string& desturi, const curl_httppost* data, rapidjson::Value& rResult, rapidjson::Document::AllocatorType& alloc, int expectedhttpcode, double timeout)
{
    MUJIN_LOG_VERBOSE(str(boost::format("POST(form) %s")%desturi));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, static_cast<long>(timeout * 1000L));
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, nullptr, desturi.data());
    _buffer.clear();
    _buffer.str("");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, nullptr, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, nullptr, &_buffer);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPPOST, nullptr, data);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, nullptr, _httpheadersmultipartformdata);
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( _buffer.rdbuf()->in_avail() > 0 ) {
        ParseJson(rResult, alloc, _buffer);
    } else {
        rResult.SetObject();
    }
    if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
        const std::string error_message = GetJsonValueByKey<std::string>(rResult, "error_message");
        throw MUJIN_EXCEPTION_FORMAT("HTTP POST to '%s' returned HTTP status %s: %s", desturi%http_code%error_message, MEC_HTTPServer);
    }
    return static_cast<int>(http_code);
}

int ControllerClientImpl::CallPost_UTF8(const std::string& relativeuri, const std::string& data, rapidjson::Document& pt, int expectedhttpcode, double timeout)
{
    return CallPost(relativeuri, encoding::ConvertUTF8ToFileSystemEncoding(data), pt, expectedhttpcode, timeout);
}

int ControllerClientImpl::CallPost_UTF16(const std::string& relativeuri, const std::wstring& data, rapidjson::Document& pt, int expectedhttpcode, double timeout)
{
    return CallPost(relativeuri, encoding::ConvertUTF16ToFileSystemEncoding(data), pt, expectedhttpcode, timeout);
}

int ControllerClientImpl::_CallPut(const std::string& relativeuri, const void* pdata, size_t nDataSize, rapidjson::Document& pt, curl_slist* headers, int expectedhttpcode, double timeout)
{
    MUJIN_LOG_DEBUG(str(boost::format("PUT %s%s")%_baseapiuri%relativeuri));
    boost::mutex::scoped_lock lock(_mutex);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, headers);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, (long)(timeout * 1000L));
    _uri = _baseapiuri;
    _uri += relativeuri;
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, _uri.c_str());
    _buffer.clear();
    _buffer.str("");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &_buffer);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_CUSTOMREQUEST, NULL, "PUT");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_POSTFIELDSIZE, 0, nDataSize);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_POSTFIELDS, NULL, pdata);
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( _buffer.rdbuf()->in_avail() > 0 ) {
        ParseJson(pt, _buffer.str());
    } else {
        pt.SetObject();
    }
    if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
        std::string error_message = GetJsonValueByKey<std::string>(pt, "error_message");
        throw MUJIN_EXCEPTION_FORMAT("HTTP PUT to '%s' returned HTTP status %s: %s", relativeuri%http_code%error_message, MEC_HTTPServer);
    }
    return http_code;
}

int ControllerClientImpl::CallPutSTL(const std::string& relativeuri, const std::vector<unsigned char>& data, rapidjson::Document& pt, int expectedhttpcode, double timeout)
{
    return _CallPut(relativeuri, static_cast<const void*> (&data[0]), data.size(), pt, _httpheadersstl, expectedhttpcode, timeout);
}

int ControllerClientImpl::CallPutJSON(const std::string& relativeuri, const std::string& data, rapidjson::Document& pt, int expectedhttpcode, double timeout)
{
    return _CallPut(relativeuri, static_cast<const void*>(&data[0]), data.size(), pt, _httpheadersjson, expectedhttpcode, timeout);
}

void ControllerClientImpl::CallDelete(const std::string& relativeuri, int expectedhttpcode, double timeout)
{
    MUJIN_LOG_DEBUG(str(boost::format("DELETE %s%s")%_baseapiuri%relativeuri));
    boost::mutex::scoped_lock lock(_mutex);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, (long)(timeout * 1000L));
    _uri = _baseapiuri;
    _uri += relativeuri;
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, _uri.c_str());
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_CUSTOMREQUEST, NULL, "DELETE");
    _buffer.clear();
    _buffer.str("");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &_buffer);
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( http_code != expectedhttpcode ) {
        rapidjson::Document d;
        ParseJson(d, _buffer.str());
        std::string error_message = GetJsonValueByKey<std::string>(d, "error_message");
        throw MUJIN_EXCEPTION_FORMAT("HTTP DELETE to '%s' returned HTTP status %s: %s", relativeuri%http_code%error_message, MEC_HTTPServer);
    }
}

std::stringstream& ControllerClientImpl::GetBuffer()
{
    return _buffer;
}

void ControllerClientImpl::SetDefaultSceneType(const std::string& scenetype)
{
    _defaultscenetype = scenetype;
}

const std::string& ControllerClientImpl::GetDefaultSceneType()
{
    return _defaultscenetype;
}

void ControllerClientImpl::SetDefaultTaskType(const std::string& tasktype)
{
    _defaulttasktype = tasktype;
}

const std::string& ControllerClientImpl::GetDefaultTaskType()
{
    return _defaulttasktype;
}

std::string ControllerClientImpl::GetScenePrimaryKeyFromURI_UTF8(const std::string& uri)
{
    size_t index = uri.find(":/");
    if (index == std::string::npos) {
        throw MUJIN_EXCEPTION_FORMAT("bad URI: %s", uri, MEC_InvalidArguments);
    }
    return EscapeString(uri.substr(index+2));
}

std::string ControllerClientImpl::GetScenePrimaryKeyFromURI_UTF16(const std::wstring& uri)
{
    std::string utf8line;
    utf8::utf16to8(uri.begin(), uri.end(), std::back_inserter(utf8line));
    return GetScenePrimaryKeyFromURI_UTF8(utf8line);
}

std::string ControllerClientImpl::GetPrimaryKeyFromName_UTF8(const std::string& name)
{
    return EscapeString(name);
}

std::string ControllerClientImpl::GetPrimaryKeyFromName_UTF16(const std::wstring& name)
{
    std::string name_utf8;
    utf8::utf16to8(name.begin(), name.end(), std::back_inserter(name_utf8));
    return GetPrimaryKeyFromName_UTF8(name_utf8);
}

std::string ControllerClientImpl::GetNameFromPrimaryKey_UTF8(const std::string& pk)
{
    return UnescapeString(pk);
}

std::wstring ControllerClientImpl::GetNameFromPrimaryKey_UTF16(const std::string& pk)
{
    std::string utf8 = GetNameFromPrimaryKey_UTF8(pk);
    std::wstring utf16;
    utf8::utf8to16(utf8.begin(), utf8.end(), std::back_inserter(utf16));
    return utf16;
}

std::string ControllerClientImpl::CreateObjectGeometry(const std::string& objectPk, const std::string& geometryName, const std::string& linkPk, const std::string& geomtype, double timeout)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    const std::string geometryData("{\"name\":\"" + geometryName + "\", \"linkpk\":\"" + linkPk + "\", \"geomtype\": \"" + geomtype + "\"}");
    const std::string uri(str(boost::format("object/%s/geometry/") % objectPk));

    CallPost(uri, geometryData, pt, 201, timeout);
    return GetJsonValueByKey<std::string>(pt, "pk");
}

std::string ControllerClientImpl::CreateIkParam(const std::string& objectPk, const std::string& name, const std::string& iktype, double timeout)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    const std::string ikparamData("{\"name\":\"" + name + "\", \"iktype\":\"" + iktype + "\"}");
    const std::string uri(str(boost::format("object/%s/ikparam/") % objectPk));

    CallPost(uri, ikparamData, pt, 201, timeout);
    return GetJsonValueByKey<std::string>(pt, "pk");
}

std::string ControllerClientImpl::CreateLink(const std::string& objectPk, const std::string& parentlinkPk, const std::string& name, const Real quaternion[4], const Real translate[3], double timeout)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    std::string data(str(boost::format("{\"name\":\"%s\", \"quaternion\":[%.15f,%.15f,%.15f,%.15f], \"translate\":[%.15f,%.15f,%.15f]")%name%quaternion[0]%quaternion[1]%quaternion[2]%quaternion[3]%translate[0]%translate[1]%translate[2]));
    if (!parentlinkPk.empty()) {
        data += ", \"parentlinkpk\": \"" + parentlinkPk + "\"";
    }
    data += "}";
    const std::string uri(str(boost::format("object/%s/link/") % objectPk));

    CallPost(uri, data, pt, 201, timeout);
    return GetJsonValueByKey<std::string>(pt, "pk");
}

std::string ControllerClientImpl::SetObjectGeometryMesh(const std::string& objectPk, const std::string& geometryPk, const std::vector<unsigned char>& meshData, const std::string& unit, double timeout)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    const std::string uri(str(boost::format("object/%s/geometry/%s/?unit=%s")%objectPk%geometryPk%unit));
    CallPutSTL(uri, meshData, pt, 202, timeout);
    return GetJsonValueByKey<std::string>(pt, "pk");
}

int ControllerClientImpl::_WriteStringStreamCallback(char *data, size_t size, size_t nmemb, std::stringstream *writerData)
{
    if (writerData == NULL) {
        return 0;
    }
    writerData->write(data, size*nmemb);
    return size * nmemb;
}

int ControllerClientImpl::_WriteOStreamCallback(char *data, size_t size, size_t nmemb, std::ostream *writerData)
{
    if (writerData == NULL) {
        return 0;
    }
    writerData->write(data, size*nmemb);
    return size * nmemb;
}

int ControllerClientImpl::_WriteVectorCallback(char *data, size_t size, size_t nmemb, std::vector<unsigned char> *writerData)
{
    if (writerData == NULL) {
        return 0;
    }
    writerData->insert(writerData->end(), data, data+size*nmemb);
    return size * nmemb;
}

int ControllerClientImpl::_ReadIStreamCallback(char *data, size_t size, size_t nmemb, std::istream *readerData)
{
    if (readerData == NULL) {
        return 0;
    }
    return readerData->read(data, size*nmemb).gcount();
}

void ControllerClientImpl::_SetupHTTPHeadersJSON()
{
    // set the header to only send json
    std::string s = std::string("Content-Type: application/json; charset=") + _charset;
    if( !!_httpheadersjson ) {
        curl_slist_free_all(_httpheadersjson);
    }
    _httpheadersjson = curl_slist_append(NULL, s.c_str());
    s = str(boost::format("Accept-Language: %s,en-us")%_language);
    _httpheadersjson = curl_slist_append(_httpheadersjson, s.c_str()); //,en;q=0.7,ja;q=0.3',")
    s = str(boost::format("Accept-Charset: %s")%_charset);
    _httpheadersjson = curl_slist_append(_httpheadersjson, s.c_str());
    //_httpheadersjson = curl_slist_append(_httpheadersjson, "Accept:"); // necessary?
    s = std::string("X-CSRFToken: ")+_csrfmiddlewaretoken;
    _httpheadersjson = curl_slist_append(_httpheadersjson, s.c_str());
    _httpheadersjson = curl_slist_append(_httpheadersjson, "Connection: Keep-Alive");
    _httpheadersjson = curl_slist_append(_httpheadersjson, "Keep-Alive: 20"); // keep alive for 20s?
    // test on windows first
    //_httpheadersjson = curl_slist_append(_httpheadersjson, "Accept-Encoding: gzip, deflate");
    for (const std::string& additionalHeader : _additionalHeaders) {
        _httpheadersjson = curl_slist_append(_httpheadersjson, additionalHeader.c_str());
    }
}

void ControllerClientImpl::_SetupHTTPHeadersSTL()
{
    // set the header to only send stl
    std::string s = std::string("Content-Type: application/sla");
    if( !!_httpheadersstl ) {
        curl_slist_free_all(_httpheadersstl);
    }
    _httpheadersstl = curl_slist_append(NULL, s.c_str());
    //_httpheadersstl = curl_slist_append(_httpheadersstl, "Accept:"); // necessary?
    s = std::string("X-CSRFToken: ")+_csrfmiddlewaretoken;
    _httpheadersstl = curl_slist_append(_httpheadersstl, s.c_str());
    _httpheadersstl = curl_slist_append(_httpheadersstl, "Connection: Keep-Alive");
    _httpheadersstl = curl_slist_append(_httpheadersstl, "Keep-Alive: 20"); // keep alive for 20s?
    // test on windows first
    //_httpheadersstl = curl_slist_append(_httpheadersstl, "Accept-Encoding: gzip, deflate");
    for (const std::string& additionalHeader : _additionalHeaders) {
        _httpheadersstl = curl_slist_append(_httpheadersstl, additionalHeader.c_str());
    }
}

void ControllerClientImpl::_SetupHTTPHeadersMultipartFormData()
{
    // set the header to only send stl
    std::string s = std::string("Content-Type: multipart/form-data");
    if( !!_httpheadersmultipartformdata ) {
        curl_slist_free_all(_httpheadersmultipartformdata);
    }
    _httpheadersmultipartformdata = curl_slist_append(NULL, s.c_str());
    //_httpheadersmultipartformdata = curl_slist_append(_httpheadersmultipartformdata, "Accept:"); // necessary?
    s = std::string("X-CSRFToken: ")+_csrfmiddlewaretoken;
    _httpheadersmultipartformdata = curl_slist_append(_httpheadersmultipartformdata, s.c_str());
    _httpheadersmultipartformdata = curl_slist_append(_httpheadersmultipartformdata, "Connection: Keep-Alive");
    _httpheadersmultipartformdata = curl_slist_append(_httpheadersmultipartformdata, "Keep-Alive: 20"); // keep alive for 20s?
    // test on windows first
    //_httpheadersmultipartformdata = curl_slist_append(_httpheadersmultipartformdata, "Accept-Encoding: gzip, deflate");
    for (const std::string& additionalHeader : _additionalHeaders) {
        _httpheadersmultipartformdata = curl_slist_append(_httpheadersmultipartformdata, additionalHeader.c_str());
    }
}

std::string ControllerClientImpl::_EncodeWithoutSeparator(const std::string& raw)
{
    std::string output;
    size_t startindex = 0;
    for(size_t i = 0; i < raw.size(); ++i) {
        if( raw[i] == '/' ) {
            if( startindex != i ) {
                output += EscapeString(raw.substr(startindex, i-startindex));
                startindex = i+1;
            }
            output += '/';
        }
    }
    if( startindex != raw.size() ) {
        output += EscapeString(raw.substr(startindex));
    }
    return output;
}

void ControllerClientImpl::_EnsureWebDAVDirectories(const std::string& relativeuri, double timeout)
{
    if (relativeuri.empty()) {
        return;
    }

    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEOUT_MS, 0L, (long)(timeout * 1000L));
    std::list<std::string> listCreateDirs;
    std::string output;
    size_t startindex = 0;
    for(size_t i = 0; i < relativeuri.size(); ++i) {
        if( relativeuri[i] == '/' ) {
            if( startindex != i ) {
                listCreateDirs.push_back(EscapeString(relativeuri.substr(startindex, i-startindex)));
                startindex = i+1;
            }
        }
    }
    if( startindex != relativeuri.size() ) {
        listCreateDirs.push_back(EscapeString(relativeuri.substr(startindex)));
    }

    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &_buffer);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_CUSTOMREQUEST, NULL, "MKCOL");

    std::string totaluri = "";
    for(std::list<std::string>::iterator itdir = listCreateDirs.begin(); itdir != listCreateDirs.end(); ++itdir) {
        // first have to create the directory structure up to destinationdir
        if( totaluri.size() > 0 ) {
            totaluri += '/';
        }
        totaluri += *itdir;
        _uri = _basewebdavuri + totaluri;
        CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
        CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, _uri.c_str());
        _buffer.clear();
        _buffer.str("");
        CURL_PERFORM(_curl);
        long http_code = 0;
        CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
        /* creating directories

           Responses from a MKCOL request MUST NOT be cached as MKCOL has non-idempotent semantics.

           201 (Created) - The collection or structured resource was created in its entirety.

           403 (Forbidden) - This indicates at least one of two conditions: 1) the server does not allow the creation of collections at the given location in its namespace, or 2) the parent collection of the Request-URI exists but cannot accept members.

           405 (Method Not Allowed) - MKCOL can only be executed on a deleted/non-existent resource.

           409 (Conflict) - A collection cannot be made at the Request-URI until one or more intermediate collections have been created.

           415 (Unsupported Media Type)- The server does not support the request type of the body.

           507 (Insufficient Storage) - The resource does not have sufficient space to record the state of the resource after the execution of this method.

         */
        if( http_code != 201 && http_code != 301 ) {
            throw MUJIN_EXCEPTION_FORMAT("HTTP MKCOL failed with HTTP status %d: %s", http_code%_errormessage, MEC_HTTPServer);
        }
    }
}

std::string ControllerClientImpl::_PrepareDestinationURI_UTF8(const std::string& rawuri, bool bEnsurePath, bool bEnsureSlash, bool bIsDirectory)
{
    std::string baseuploaduri;
    if( rawuri.size() >= 7 && rawuri.substr(0,7) == "mujin:/" ) {
        baseuploaduri = _basewebdavuri;
        std::string s = rawuri.substr(7);
        baseuploaduri += _EncodeWithoutSeparator(s);
        if( bEnsurePath ) {
            if( !bIsDirectory ) {
                size_t nBaseFilenameStartIndex = s.find_last_of(s_filesep);
                if( nBaseFilenameStartIndex != std::string::npos ) {
                    s = s.substr(0, nBaseFilenameStartIndex);
                } else {
                    s = "";
                }
            }
            _EnsureWebDAVDirectories(s);
        }
    }
    else {
        if( !bEnsureSlash ) {
            return rawuri;
        }
        baseuploaduri = rawuri;
    }
    if( bEnsureSlash ) {
        // ensure trailing slash
        if( baseuploaduri[baseuploaduri.size()-1] != '/' ) {
            baseuploaduri.push_back('/');
        }
    }
    return baseuploaduri;
}

std::string ControllerClientImpl::_PrepareDestinationURI_UTF16(const std::wstring& rawuri_utf16, bool bEnsurePath, bool bEnsureSlash, bool bIsDirectory)
{
    std::string baseuploaduri;
    std::string desturi_utf8;
    utf8::utf16to8(rawuri_utf16.begin(), rawuri_utf16.end(), std::back_inserter(desturi_utf8));

    if( desturi_utf8.size() >= 7 && desturi_utf8.substr(0,7) == "mujin:/" ) {
        baseuploaduri = _basewebdavuri;
        std::string s = desturi_utf8.substr(7);
        baseuploaduri += _EncodeWithoutSeparator(s);
        if( bEnsurePath ) {
            if( !bIsDirectory ) {
                size_t nBaseFilenameStartIndex = s.find_last_of(s_filesep);
                if( nBaseFilenameStartIndex != std::string::npos ) {
                    s = s.substr(0, nBaseFilenameStartIndex);
                } else {
                    s = "";
                }
            }
            _EnsureWebDAVDirectories(s);
        }
    }
    else {
        if( !bEnsureSlash ) {
            return desturi_utf8;
        }
        baseuploaduri = desturi_utf8;
    }
    if( bEnsureSlash ) {
        // ensure trailing slash
        if( baseuploaduri[baseuploaduri.size()-1] != '/' ) {
            baseuploaduri.push_back('/');
        }
    }
    return baseuploaduri;
}

void ControllerClientImpl::UploadFileToController_UTF8(const std::string& filename, const std::string& desturi)
{
    boost::mutex::scoped_lock lock(_mutex);
    _UploadFileToController_UTF8(filename, _PrepareDestinationURI_UTF8(desturi, false));
}

void ControllerClientImpl::UploadFileToController_UTF16(const std::wstring& filename_utf16, const std::wstring& desturi_utf16)
{
    boost::mutex::scoped_lock lock(_mutex);
    _UploadFileToController_UTF16(filename_utf16, _PrepareDestinationURI_UTF16(desturi_utf16, false));
}

void ControllerClientImpl::UploadDataToController_UTF8(const void* data, size_t size, const std::string& desturi)
{
    boost::mutex::scoped_lock lock(_mutex);
    const std::string filename = _PrepareDestinationURI_UTF8(desturi, false).substr(_basewebdavuri.size());
    _UploadDataToControllerViaForm(data, size, filename, _baseuri + "fileupload");
}

void ControllerClientImpl::UploadDataToController_UTF16(const void* data, size_t size, const std::wstring& desturi)
{
    boost::mutex::scoped_lock lock(_mutex);
    const std::string filename = _PrepareDestinationURI_UTF16(desturi, false).substr(_basewebdavuri.size());
    _UploadDataToControllerViaForm(data, size, filename, _baseuri + "fileupload");
}

void ControllerClientImpl::UploadDirectoryToController_UTF8(const std::string& copydir, const std::string& desturi)
{
    boost::mutex::scoped_lock lock(_mutex);
    _UploadDirectoryToController_UTF8(copydir, _PrepareDestinationURI_UTF8(desturi, false, false, true));
}

void ControllerClientImpl::UploadDirectoryToController_UTF16(const std::wstring& copydir, const std::wstring& desturi)
{
    boost::mutex::scoped_lock lock(_mutex);
    _UploadDirectoryToController_UTF16(copydir, _PrepareDestinationURI_UTF16(desturi, false, false, true));
}

void ControllerClientImpl::DownloadFileFromController_UTF8(const std::string& desturi, std::vector<unsigned char>& vdata)
{
    boost::mutex::scoped_lock lock(_mutex);
    _CallGet(_PrepareDestinationURI_UTF8(desturi, false), vdata);
}

void ControllerClientImpl::DownloadFileFromController_UTF16(const std::wstring& desturi, std::vector<unsigned char>& vdata)
{
    boost::mutex::scoped_lock lock(_mutex);
    _CallGet(_PrepareDestinationURI_UTF16(desturi, false), vdata);
}

void ControllerClientImpl::DownloadFileFromControllerIfModifiedSince_UTF8(const std::string& desturi, long localtimeval, long& remotetimeval, std::vector<unsigned char>& vdata, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    _DownloadFileFromController(_PrepareDestinationURI_UTF8(desturi, false), localtimeval, remotetimeval, vdata, timeout);
}

void ControllerClientImpl::DownloadFileFromControllerIfModifiedSince_UTF16(const std::wstring& desturi, long localtimeval, long& remotetimeval, std::vector<unsigned char>& vdata, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    _DownloadFileFromController(_PrepareDestinationURI_UTF16(desturi, false), localtimeval, remotetimeval, vdata, timeout);
}

long ControllerClientImpl::GetModifiedTime(const std::string& uri, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);

    // Copied from https://curl.haxx.se/libcurl/c/CURLINFO_FILETIME.html
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);

    // in order to resolve cache correctly, need to go thorugh file/download endpoint
    std::string apiendpoint = _baseuri + "file/download/?filename=";
    if( uri.size() >= 7 && uri.substr(0,7) == "mujin:/" ) {
        apiendpoint += _EncodeWithoutSeparator(uri.substr(7));
    }

    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, apiendpoint.c_str());
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_FILETIME, 0L, 1L);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_NOBODY, 0L, 1L);
    CURL_PERFORM(_curl);

    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( http_code != 200 ) {
        throw MUJIN_EXCEPTION_FORMAT("Cannot get modified date of %s for HTTP HEAD call: return is %s", uri%http_code, MEC_HTTPServer);
    }

    long filetime=-1;
    CURL_INFO_GETTER(_curl, CURLINFO_FILETIME, &filetime);
    return filetime;
}

void ControllerClientImpl::_DownloadFileFromController(const std::string& desturi, long localtimeval, long &remotetimeval, std::vector<unsigned char>& outputdata, double timeout)
{
    remotetimeval = 0;

    // ask for remote file time
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_FILETIME, 0L, 1L);

    // use if modified since if local file time is provided
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE, localtimeval > 0 ? CURL_TIMECOND_IFMODSINCE : CURL_TIMECOND_NONE);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_TIMEVALUE, 0L, localtimeval > 0 ? localtimeval : 0L);

    // do the get call
    long http_code = _CallGet(desturi, outputdata, 0, timeout);
    if ((http_code != 200 && http_code != 304)) {
        if (outputdata.size() > 0) {
            std::stringstream ss;
            ss.write((const char*)&outputdata[0], outputdata.size());
            throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s: %s", desturi%http_code%ss.str(), MEC_HTTPServer);
        }
        throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s", desturi%http_code, MEC_HTTPServer);
    }

    // retrieve remote file time
    if (http_code != 304) {
        // got the entire file so fill in the timestamp of that file
        CURL_INFO_GETTER(_curl, CURLINFO_FILETIME, &remotetimeval);
    }
}

void ControllerClientImpl::SaveBackup(std::ostream& outputStream, bool config, bool media, const std::string& backupscenepks, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    std::string query=std::string("?config=")+(config ? "true" : "false")+"&media="+(media ? "true" : "false")+"&backupscenepks="+backupscenepks;
    _CallGet(_baseuri+"backup/"+query, outputStream, 200, timeout);
}

void ControllerClientImpl::RestoreBackup(std::istream& inputStream, bool config, bool media, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    std::string query=std::string("?config=")+(config ? "true" : "false")+"&media="+(media ? "true" : "false");
    _UploadFileToControllerViaForm(inputStream, "", _baseuri+"backup/"+query, timeout);
}

void ControllerClientImpl::Upgrade(std::istream& inputStream, bool autorestart, bool uploadonly, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    std::string query=std::string("?autorestart=")+(autorestart ? "1" : "0")+("&uploadonly=")+(uploadonly ? "1" : "0");

    std::streampos originalPos = inputStream.tellg();
    inputStream.seekg(0, std::ios::end);
    if(inputStream.fail()) {
        throw MUJIN_EXCEPTION_FORMAT0("failed to seek inputStream to get the length", MEC_InvalidArguments);
    }
    std::streampos contentLength = inputStream.tellg() - originalPos;
    if(inputStream.fail()) {
        throw MUJIN_EXCEPTION_FORMAT0("failed to tell the length of inputStream", MEC_InvalidArguments);
    }
    inputStream.seekg(originalPos, std::ios::beg);
    if(inputStream.fail()) {
        throw MUJIN_EXCEPTION_FORMAT0("failed to rewind inputStream", MEC_InvalidArguments);
    }

    if(contentLength) {
        _UploadFileToControllerViaForm(inputStream, "", _baseuri+"upgrade/"+query, timeout);
    } else {
        rapidjson::Document pt(rapidjson::kObjectType);
        _CallPost(_baseuri+"upgrade/"+query, "", pt, pt.GetAllocator(), 200, timeout);
    }
}

bool ControllerClientImpl::GetUpgradeStatus(std::string& status, double &progress, double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    rapidjson::Document pt(rapidjson::kObjectType);
    _CallGet(_baseuri+"upgrade/", pt, pt.GetAllocator(), 200, timeout);
    if(pt.IsNull()) {
        return false;
    }
    status = GetJsonValueByKey<std::string>(pt, "status");
    progress = GetJsonValueByKey<double>(pt, "progress");
    return true;
}

void ControllerClientImpl::CancelUpgrade(double timeout)
{
    CallDelete(_baseuri+"upgrade/", 200, timeout);
}

void ControllerClientImpl::Reboot(double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    rapidjson::Document pt(rapidjson::kObjectType);
    _CallPost(_baseuri+"reboot/", "", pt, pt.GetAllocator(), 200, timeout);
}

void ControllerClientImpl::DeleteAllScenes(double timeout)
{
    boost::mutex::scoped_lock lock(_mutex);
    rapidjson::Document pt(rapidjson::kObjectType);
    CallDelete("scene/", 204, timeout);
}

void ControllerClientImpl::DeleteAllITLPrograms(double timeout)
{
    CallDelete("itl/", 204, timeout);
}

void ControllerClientImpl::DeleteFileOnController_UTF8(const std::string& desturi)
{
    boost::mutex::scoped_lock lock(_mutex);
    _DeleteFileOnController(_PrepareDestinationURI_UTF8(desturi, false));
}

void ControllerClientImpl::DeleteFileOnController_UTF16(const std::wstring& desturi)
{
    boost::mutex::scoped_lock lock(_mutex);
    _DeleteFileOnController(_PrepareDestinationURI_UTF16(desturi, false));
}

void ControllerClientImpl::DeleteDirectoryOnController_UTF8(const std::string& desturi)
{
    boost::mutex::scoped_lock lock(_mutex);
    _DeleteDirectoryOnController(_PrepareDestinationURI_UTF8(desturi, false, false, true));
}

void ControllerClientImpl::DeleteDirectoryOnController_UTF16(const std::wstring& desturi)
{
    boost::mutex::scoped_lock lock(_mutex);
    _DeleteDirectoryOnController(_PrepareDestinationURI_UTF16(desturi, false, false, true));
}

void ControllerClientImpl::ModifySceneAddReferenceObjectPK(const std::string &scenepk, const std::string &referenceobjectpk, double timeout)
{
    rapidjson::Document pt, pt2;
    rapidjson::Value value;

    pt.SetObject();

    value.SetString(scenepk.c_str(), pt.GetAllocator());
    pt.AddMember("scenepk", value, pt.GetAllocator());

    value.SetString(referenceobjectpk.c_str(), pt.GetAllocator());
    pt.AddMember("referenceobjectpk", value, pt.GetAllocator());

    boost::mutex::scoped_lock lock(_mutex);
    _CallPost(_baseuri + "referenceobjectpks/add/", DumpJson(pt), pt2, pt2.GetAllocator(), 200, timeout);
}

void ControllerClientImpl::ModifySceneRemoveReferenceObjectPK(const std::string &scenepk, const std::string &referenceobjectpk, double timeout)
{
    rapidjson::Document pt, pt2;
    rapidjson::Value value;

    pt.SetObject();

    value.SetString(scenepk.c_str(), pt.GetAllocator());
    pt.AddMember("scenepk", value, pt.GetAllocator());

    value.SetString(referenceobjectpk.c_str(), pt.GetAllocator());
    pt.AddMember("referenceobjectpk", value, pt.GetAllocator());

    boost::mutex::scoped_lock lock(_mutex);
    _CallPost(_baseuri + "referenceobjectpks/remove/", DumpJson(pt), pt2, pt2.GetAllocator(), 200, timeout);
}

void ControllerClientImpl::_UploadDirectoryToController_UTF8(const std::string& copydir_utf8, const std::string& rawuri)
{
    BOOST_ASSERT(rawuri.size()>0 && copydir_utf8.size()>0);

    // if there's a trailing slash, have to get rid of it
    std::string uri;
    if( rawuri.at(rawuri.size()-1) == '/' ) {
        if( copydir_utf8.at(copydir_utf8.size()-1) != s_filesep ) {
            // append the copydir_utf8 name to rawuri
            size_t nBaseFilenameStartIndex = copydir_utf8.find_last_of(s_filesep);
            if( nBaseFilenameStartIndex == std::string::npos ) {
                // there's no path?
                nBaseFilenameStartIndex = 0;
            }
            else {
                nBaseFilenameStartIndex++;
            }
            uri = rawuri + EscapeString(copydir_utf8.substr(nBaseFilenameStartIndex));
        }
        else {
            // copydir also ends in a fileseparator, so remove the file separator from rawuri
            uri = rawuri.substr(0, rawuri.size()-1);
        }
    }
    else {
        if (copydir_utf8.at(copydir_utf8.size()-1) == s_filesep) {
            throw MUJIN_EXCEPTION_FORMAT("copydir '%s' cannot end in slash '%s'", copydir_utf8%s_filesep, MEC_InvalidArguments);
        }
        uri = rawuri;
    }

    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &_buffer);

    {
        // make sure the directory is created
        CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_CUSTOMREQUEST, NULL, "MKCOL");
        CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
        CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, uri.c_str());
        CURL_PERFORM(_curl);
        long http_code = 0;
        CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
        if( http_code != 201 && http_code != 301 ) {
            throw MUJIN_EXCEPTION_FORMAT("HTTP MKCOL failed for %s with HTTP status %d: %s", uri%http_code%_errormessage, MEC_HTTPServer);
        }
    }

    std::string sCopyDir_FS = encoding::ConvertUTF8ToFileSystemEncoding(copydir_utf8);
    // remove the fileseparator if it exists
    std::stringstream ss;
    ss << "uploading " << sCopyDir_FS << " -> " << uri;
    MUJIN_LOG_INFO(ss.str());

#if defined(_WIN32) || defined(_WIN64)
    bool bhasseparator = false;
    if( sCopyDir_FS.size() > 0 && sCopyDir_FS.at(sCopyDir_FS.size()-1) == s_filesep ) {
        sCopyDir_FS.resize(sCopyDir_FS.size()-1);
        bhasseparator = true;
    }

    WIN32_FIND_DATAA ffd;
    std::string searchstr = sCopyDir_FS + std::string("\\*");
    HANDLE hFind = FindFirstFileA(searchstr.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        throw MUJIN_EXCEPTION_FORMAT("could not retrieve file data for %s", sCopyDir_FS, MEC_Assert);
    }

    do {
        std::string filename = std::string(ffd.cFileName);
        if( filename != "." && filename != ".." ) {
            std::string filename_utf8 = encoding::ConvertMBStoUTF8(filename);
            std::string newcopydir_utf8;
            if( bhasseparator ) {
                newcopydir_utf8 = copydir_utf8 + filename_utf8;
            }
            else {
                newcopydir_utf8 = str(boost::format("%s%c%s")%copydir_utf8%s_filesep%filename_utf8);
            }
            std::string newuri = str(boost::format("%s/%s")%uri%EscapeString(filename_utf8));

            if( ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
                _UploadDirectoryToController_UTF8(newcopydir_utf8, newuri);
            }
            else if( ffd.dwFileAttributes == 0 || ffd.dwFileAttributes == FILE_ATTRIBUTE_READONLY || ffd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL || ffd.dwFileAttributes == FILE_ATTRIBUTE_ARCHIVE ) {
                _UploadFileToController_UTF8(newcopydir_utf8, newuri);
            }
        }
    } while(FindNextFileA(hFind,&ffd) != 0);

    DWORD err = GetLastError();
    FindClose(hFind);
    if( err != ERROR_NO_MORE_FILES ) {
        throw MUJIN_EXCEPTION_FORMAT("system error 0x%x when recursing through %s", err%sCopyDir_FS, MEC_HTTPServer);
    }

#else
    boost::filesystem::path bfpcopydir(copydir_utf8);
    for(boost::filesystem::directory_iterator itdir(bfpcopydir); itdir != boost::filesystem::directory_iterator(); ++itdir) {
#if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
        std::string dirfilename = encoding::ConvertFileSystemEncodingToUTF8(itdir->path().filename().string());
#else
        std::string dirfilename = encoding::ConvertFileSystemEncodingToUTF8(itdir->path().filename());
#endif
        std::string newuri = str(boost::format("%s/%s")%uri%EscapeString(dirfilename));
        if( boost::filesystem::is_directory(itdir->status()) ) {
            _UploadDirectoryToController_UTF8(itdir->path().string(), newuri);
        }
        else if( boost::filesystem::is_regular_file(itdir->status()) ) {
            _UploadFileToController_UTF8(itdir->path().string(), newuri);
        }
    }
#endif // defined(_WIN32) || defined(_WIN64)
}

void ControllerClientImpl::_UploadDirectoryToController_UTF16(const std::wstring& copydir_utf16, const std::string& rawuri)
{
    BOOST_ASSERT(rawuri.size()>0 && copydir_utf16.size()>0);

    // if there's a trailing slash, have to get rid of it
    std::string uri;
    if( rawuri.at(rawuri.size()-1) == '/' ) {
        if( copydir_utf16.at(copydir_utf16.size()-1) != s_wfilesep ) {
            // append the copydir_utf16 name to rawuri
            size_t nBaseFilenameStartIndex = copydir_utf16.find_last_of(s_wfilesep);
            if( nBaseFilenameStartIndex == std::string::npos ) {
                // there's no path?
                nBaseFilenameStartIndex = 0;
            }
            else {
                nBaseFilenameStartIndex++;
            }
            std::string name_utf8;
            utf8::utf16to8(copydir_utf16.begin()+nBaseFilenameStartIndex, copydir_utf16.end(), std::back_inserter(name_utf8));
            uri = rawuri + EscapeString(name_utf8);
        }
        else {
            // copydir also ends in a fileseparator, so remove the file separator from rawuri
            uri = rawuri.substr(0, rawuri.size()-1);
        }
    }
    else {
        if (copydir_utf16.at(copydir_utf16.size()-1) == s_wfilesep) {
            throw MUJIN_EXCEPTION_FORMAT("copydir '%s' cannot end in slash '%s'", encoding::ConvertUTF16ToFileSystemEncoding(copydir_utf16)%s_filesep, MEC_InvalidArguments);
        }
        uri = rawuri;
    }

    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEFUNCTION, NULL, _WriteStringStreamCallback);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_WRITEDATA, NULL, &_buffer);

    {
        // make sure the directory is created
        CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_CUSTOMREQUEST, NULL, "MKCOL");
        CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
        CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, uri.c_str());
        CURL_PERFORM(_curl);
        long http_code = 0;
        CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
        if( http_code != 201 && http_code != 301 ) {
            throw MUJIN_EXCEPTION_FORMAT("HTTP MKCOL failed for %s with HTTP status %d: %s", uri%http_code%_errormessage, MEC_HTTPServer);
        }
    }

    std::wstring sCopyDir_FS;
    // remove the fileseparator if it exists
    if( copydir_utf16.size() > 0 && copydir_utf16.at(copydir_utf16.size()-1) == s_wfilesep ) {
        sCopyDir_FS = copydir_utf16.substr(0,copydir_utf16.size()-1);
    }
    else {
        sCopyDir_FS = copydir_utf16;
    }
    std::stringstream ss;
    ss << "uploading " << encoding::ConvertUTF16ToFileSystemEncoding(copydir_utf16) << " -> " << uri;
    MUJIN_LOG_INFO(ss.str());

#if defined(_WIN32) || defined(_WIN64)
    WIN32_FIND_DATAW ffd;
    std::wstring searchstr = sCopyDir_FS + std::wstring(L"\\*");
    HANDLE hFind = FindFirstFileW(searchstr.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)  {
        throw MUJIN_EXCEPTION_FORMAT("could not retrieve file data for %s", encoding::ConvertUTF16ToFileSystemEncoding(copydir_utf16), MEC_Assert);
    }

    do {
        std::wstring filename = std::wstring(ffd.cFileName);
        if( filename != L"." && filename != L".." ) {
            std::string filename_utf8;
            utf8::utf16to8(filename.begin(), filename.end(), std::back_inserter(filename_utf8));
            std::wstring newcopydir = str(boost::wformat(L"%s\\%s")%copydir_utf16%filename);
            std::string newuri = str(boost::format("%s/%s")%uri%EscapeString(filename_utf8));

            if( ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
                _UploadDirectoryToController_UTF16(newcopydir, newuri);
            }
            else if( ffd.dwFileAttributes == 0 || ffd.dwFileAttributes == FILE_ATTRIBUTE_READONLY || ffd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL || ffd.dwFileAttributes == FILE_ATTRIBUTE_ARCHIVE ) {
                _UploadFileToController_UTF16(newcopydir, newuri);
            }
        }
    } while(FindNextFileW(hFind,&ffd) != 0);

    DWORD err = GetLastError();
    FindClose(hFind);
    if( err !=  ERROR_NO_MORE_FILES ) {
        throw MUJIN_EXCEPTION_FORMAT("system error 0x%x when recursing through %s", err%encoding::ConvertUTF16ToFileSystemEncoding(copydir_utf16), MEC_HTTPServer);
    }

#elif defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
    boost::filesystem::path bfpcopydir(copydir_utf16);
    for(boost::filesystem::directory_iterator itdir(bfpcopydir); itdir != boost::filesystem::directory_iterator(); ++itdir) {
        std::wstring dirfilename_utf16 = itdir->path().filename().wstring();
        std::string dirfilename;
        utf8::utf16to8(dirfilename_utf16.begin(), dirfilename_utf16.end(), std::back_inserter(dirfilename));
        std::string newuri = str(boost::format("%s/%s")%uri%EscapeString(dirfilename));
        if( boost::filesystem::is_directory(itdir->status()) ) {
            _UploadDirectoryToController_UTF16(itdir->path().wstring(), newuri);
        }
        else if( boost::filesystem::is_regular_file(itdir->status()) ) {
            _UploadFileToController_UTF16(itdir->path().wstring(), newuri);
        }
    }
#else
    // boost filesystem v2
    boost::filesystem::wpath bfpcopydir(copydir_utf16);
    for(boost::filesystem::wdirectory_iterator itdir(bfpcopydir); itdir != boost::filesystem::wdirectory_iterator(); ++itdir) {
        std::wstring dirfilename_utf16 = itdir->path().filename();
        std::string dirfilename;
        utf8::utf16to8(dirfilename_utf16.begin(), dirfilename_utf16.end(), std::back_inserter(dirfilename));
        std::string newuri = str(boost::format("%s/%s")%uri%EscapeString(dirfilename));
        if( boost::filesystem::is_directory(itdir->status()) ) {
            _UploadDirectoryToController_UTF16(itdir->path().string(), newuri);
        }
        else if( boost::filesystem::is_regular_file(itdir->status()) ) {
            _UploadFileToController_UTF16(itdir->path().string(), newuri);
        }
    }
#endif // defined(_WIN32) || defined(_WIN64)
}

void ControllerClientImpl::_UploadFileToController_UTF8(const std::string& filename, const std::string& uri)
{
    // the dest filename of the upload is determined by stripping the leading _basewebdavuri
    if( uri.size() < _basewebdavuri.size() || uri.substr(0,_basewebdavuri.size()) != _basewebdavuri ) {
        throw MUJIN_EXCEPTION_FORMAT("trying to upload a file outside of the webdav endpoint is not allowed: %s", uri, MEC_HTTPServer);
    }
    std::string filenameoncontroller = uri.substr(_basewebdavuri.size());

    std::string sFilename_FS = encoding::ConvertUTF8ToFileSystemEncoding(filename);
    std::ifstream fin(sFilename_FS.c_str(), std::ios::in | std::ios::binary);
    if(!fin.good()) {
        throw MUJIN_EXCEPTION_FORMAT("failed to open filename %s for uploading", sFilename_FS, MEC_InvalidArguments);
    }

    MUJIN_LOG_DEBUG(str(boost::format("upload %s")%uri))
    _UploadFileToControllerViaForm(fin, filenameoncontroller, _baseuri + "fileupload");
}

void ControllerClientImpl::_UploadFileToController_UTF16(const std::wstring& filename, const std::string& uri)
{
    // the dest filename of the upload is determined by stripping the leading _basewebdavuri
    if( uri.size() < _basewebdavuri.size() || uri.substr(0,_basewebdavuri.size()) != _basewebdavuri ) {
        throw MUJIN_EXCEPTION_FORMAT("trying to upload a file outside of the webdav endpoint is not allowed: %s", uri, MEC_HTTPServer);
    }
    std::string filenameoncontroller = uri.substr(_basewebdavuri.size());

    std::string sFilename_FS = encoding::ConvertUTF16ToFileSystemEncoding(filename);
    std::vector<unsigned char>content;
    std::ifstream fin(sFilename_FS.c_str(), std::ios::in | std::ios::binary);
    if(!fin.good()) {
        throw MUJIN_EXCEPTION_FORMAT("failed to open filename %s for uploading", sFilename_FS, MEC_InvalidArguments);
    }

    MUJIN_LOG_DEBUG(str(boost::format("upload %s")%uri))
    _UploadFileToControllerViaForm(fin, filenameoncontroller, _baseuri + "fileupload");
}

void ControllerClientImpl::_UploadFileToControllerViaForm(std::istream& inputStream, const std::string& filename, const std::string& endpoint, double timeout)
{
    std::streampos originalPos = inputStream.tellg();
    inputStream.seekg(0, std::ios::end);
    if(inputStream.fail()) {
        throw MUJIN_EXCEPTION_FORMAT0("failed to seek inputStream to get the length", MEC_InvalidArguments);
    }
    std::streampos contentLength = inputStream.tellg() - originalPos;
    if(inputStream.fail()) {
        throw MUJIN_EXCEPTION_FORMAT0("failed to tell the length of inputStream", MEC_InvalidArguments);
    }
    inputStream.seekg(originalPos, std::ios::beg);
    if(inputStream.fail()) {
        throw MUJIN_EXCEPTION_FORMAT0("failed to rewind inputStream", MEC_InvalidArguments);
    }

    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_READFUNCTION, NULL, _ReadIStreamCallback);
    // prepare form
    struct curl_httppost *formpost = nullptr;
    struct curl_httppost *lastptr = nullptr;
    CURLFormReleaser curlFormReleaser{formpost};
    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "files[]",
                 CURLFORM_FILENAME, filename.empty() ? "unused" : filename.c_str(),
                 CURLFORM_STREAM, &inputStream,
#if !CURL_AT_LEAST_VERSION(7,46,0)
                 // According to curl/lib/formdata.c, CURLFORM_CONTENTSLENGTH argument type is long.
                 // Also, as va_list is used in curl_formadd, the bit length needs to match exactly.
                 // streampos can be directly converted to streamoff, but it does not correspond on 32bit machines.
                 CURLFORM_CONTENTSLENGTH, (long)contentLength,
#else
                 // Actually we should use CURLFORM_CONTENTLEN, whose argument type is curl_off_t, which is 64bit.
                 // However, it was added in curl 7.46 and cannot be used in official Windows build.
                 CURLFORM_CONTENTLEN, (curl_off_t)contentLength,
#endif
                 CURLFORM_END);
    if(!filename.empty()) {
        curl_formadd(&formpost, &lastptr,
                     CURLFORM_COPYNAME, "filename",
                     CURLFORM_COPYCONTENTS, filename.c_str(),
                     CURLFORM_END);
    }

    rapidjson::Document ignored;
    // 204 is when it overwrites the file?
    _CallPost(endpoint, formpost, ignored, ignored.GetAllocator(), 200, timeout);
}

void ControllerClientImpl::_UploadDataToControllerViaForm(const void* data, size_t size, const std::string& filename, const std::string& endpoint, double timeout)
{
    // prepare form
    struct curl_httppost *formpost = nullptr;
    struct curl_httppost *lastptr = nullptr;
    CURLFormReleaser curlFormReleaser{formpost};
    curl_formadd(&formpost, &lastptr,
                 CURLFORM_PTRNAME, "files[]",
                 CURLFORM_BUFFER, filename.empty() ? "unused" : filename.c_str(),
                 CURLFORM_BUFFERPTR, data,
                 CURLFORM_END);
    if(!filename.empty()) {
        curl_formadd(&formpost, &lastptr,
                     CURLFORM_PTRNAME, "filename",
                     CURLFORM_PTRCONTENTS, filename.c_str(),
                     CURLFORM_END);
    }

    rapidjson::Document ignored;
    // 204 is when it overwrites the file?
    _CallPost(endpoint, formpost, ignored, ignored.GetAllocator(), 200, timeout);
}

void ControllerClientImpl::_DeleteFileOnController(const std::string& desturi)
{
    MUJIN_LOG_DEBUG(str(boost::format("delete %s")%desturi))

    // the dest filename of the upload is determined by stripping the leading _basewebdavuri
    if( desturi.size() < _basewebdavuri.size() || desturi.substr(0,_basewebdavuri.size()) != _basewebdavuri ) {
        throw MUJIN_EXCEPTION_FORMAT("trying to upload a file outside of the webdav endpoint is not allowed: %s", desturi, MEC_HTTPServer);
    }
    std::string filename = desturi.substr(_basewebdavuri.size());

    rapidjson::Document pt(rapidjson::kObjectType);
    _CallPost(_baseuri+"file/delete/?filename="+filename, "", pt, pt.GetAllocator(), 200, 5.0);
}

void ControllerClientImpl::_DeleteDirectoryOnController(const std::string& desturi)
{
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_CUSTOMREQUEST, NULL, "DELETE");
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_HTTPHEADER, NULL, _httpheadersjson);
    CURL_OPTION_SAVE_SETTER(_curl, CURLOPT_URL, NULL, desturi.c_str());
    CURL_PERFORM(_curl);
    long http_code = 0;
    CURL_INFO_GETTER(_curl, CURLINFO_RESPONSE_CODE, &http_code);
    MUJIN_LOG_INFO("response code: " << http_code);
}

size_t ControllerClientImpl::_ReadUploadCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    // in real-world cases, this would probably get this data differently as this fread() stuff is exactly what the library already would do by default internally
    size_t nread = fread(ptr, size, nmemb, (FILE*)stream);
    //fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T " bytes from file\n", nread);
    return nread;
}

size_t ControllerClientImpl::_ReadInMemoryUploadCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    std::pair<std::vector<unsigned char>::const_iterator, size_t>* pstreamdata = static_cast<std::pair<std::vector<unsigned char>::const_iterator, size_t>*>(stream);
    size_t nBytesToRead = size*nmemb;
    if( nBytesToRead > pstreamdata->second ) {
        nBytesToRead = pstreamdata->second;
    }
    if( nBytesToRead > 0 ) {
        std::copy(pstreamdata->first, pstreamdata->first+nBytesToRead, static_cast<unsigned char*>(ptr));
        pstreamdata->first += nBytesToRead;
        pstreamdata->second -= nBytesToRead;
    }
    return nBytesToRead;
}

void ControllerClientImpl::GetDebugInfos(std::vector<DebugResourcePtr>& debuginfos, double timeout)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    CallGet(str(boost::format("debug/?format=json&limit=0")), pt, 200, timeout);
    rapidjson::Value& objects = pt["objects"];

    debuginfos.resize(objects.Size());
    size_t iobj = 0;
    for (rapidjson::Document::ValueIterator it = objects.Begin(); it != objects.End(); ++it) {
        DebugResourcePtr debuginfo(new DebugResource(shared_from_this(), GetJsonValueByKey<std::string>(*it, "pk")));

        //LoadJsonValueByKey(*it, "datemodified", debuginfo->datemodified);
        LoadJsonValueByKey(*it, "description", debuginfo->description);
        //LoadJsonValueByKey(*it, "downloadUri", debuginfo->downloadUri);
        LoadJsonValueByKey(*it, "name", debuginfo->name);
        //LoadJsonValueByKey(*it, "resource_uri", debuginfo->resource_uri);
        LoadJsonValueByKey(*it, "size", debuginfo->size);

        debuginfos.at(iobj++) = debuginfo;
    }
}

void ControllerClientImpl::ListFilesInController(std::vector<FileEntry>& fileentries, const std::string &dirname, double timeout)
{
    rapidjson::Document pt(rapidjson::kObjectType);
    _CallGet(_baseuri+"file/list/?dirname="+dirname, pt, pt.GetAllocator(), 200, timeout);
    fileentries.resize(pt.MemberCount());
    size_t iobj = 0;
    for (rapidjson::Document::MemberIterator it = pt.MemberBegin(); it != pt.MemberEnd(); ++it) {
        FileEntry &fileentry = fileentries.at(iobj);

        fileentry.filename = it->name.GetString();
        LoadJsonValueByKey(it->value, "modified", fileentry.modified);
        LoadJsonValueByKey(it->value, "size", fileentry.size);

        iobj++;
    }
}

void ControllerClientImpl::CreateLogEntries(const std::vector<LogEntry>& logEntries, std::vector<std::string>& createdLogEntryIds, double timeout)
{
    if (logEntries.empty()) {
        return;
    }

    boost::mutex::scoped_lock lock(_mutex);

    // prepare the form
    struct curl_httppost *formpost = nullptr;
    struct curl_httppost *lastptr = nullptr;
    CURLFormReleaser curlFormReleaser{formpost};

    rapidjson::StringBuffer& rRequestStringBuffer = _rRequestStringBufferCache;
    rapidjson::Writer<rapidjson::StringBuffer> writer(rRequestStringBuffer);

    for (const LogEntry &logEntry : logEntries) {
        // add log entry content
        rRequestStringBuffer.Clear();
        writer.Reset(rRequestStringBuffer);
        logEntry.rEntry.Accept(writer);
        std::string formName = "logEntry/" + logEntry.logType;
        curl_formadd(&formpost, &lastptr,
                    CURLFORM_COPYNAME, formName.c_str(),
                    CURLFORM_COPYCONTENTS, rRequestStringBuffer.GetString(),
                    CURLFORM_CONTENTTYPE, "application/json",
                    CURLFORM_END);

        // add attachments
        for (const LogEntryAttachment &attachment : logEntry.attachments) {
            curl_formadd(&formpost, &lastptr,
                CURLFORM_COPYNAME, "attachment",
                CURLFORM_BUFFER, attachment.filename.c_str(),
                CURLFORM_BUFFERPTR, attachment.data.data(),
                CURLFORM_BUFFERLENGTH, (long)(attachment.data.size()),
                CURLFORM_END);
        }
    }

    // perform the call
    rapidjson::Document rResultDoc;
    _uri = _baseuri + "api/v2/logEntry";
    _CallPost(_uri, formpost, rResultDoc, rResultDoc.GetAllocator(), 201, timeout);

    if (!rResultDoc.IsObject() || !rResultDoc.HasMember("logEntryIds") || !rResultDoc["logEntryIds"].IsArray()) {
        throw MUJIN_EXCEPTION_FORMAT("invalid response received while uploading log entries: %s", mujinjson::DumpJson(rResultDoc), MEC_HTTPServer);
    }
    
    // exract the log entry ids
    const rapidjson::Value& logEntryIdsArray = rResultDoc["logEntryIds"];
    for (rapidjson::Value::ConstValueIterator itLogEntryId = logEntryIdsArray.Begin(); itLogEntryId != logEntryIdsArray.End(); ++itLogEntryId) {
        if (itLogEntryId->IsString()) {
            createdLogEntryIds.emplace_back(itLogEntryId->GetString());
        }
    }
}

GraphSubscriptionHandlerImpl::GraphSubscriptionHandlerImpl(GraphSubscriptionWebSocketHandlerPtr graphSubscriptionWebSocketHandler, std::string subscriptionId)
: _graphSubscriptionWebSocketHandler(graphSubscriptionWebSocketHandler), _subscriptionId(subscriptionId)
{

}

GraphSubscriptionHandlerImpl::~GraphSubscriptionHandlerImpl()
{
    // gracefully complete the subscription
    _graphSubscriptionWebSocketHandler->StopSubscription(_subscriptionId);
}

template <typename Socket>
void _ReadFromSubscriptionStream(boost::shared_ptr<boost::beast::websocket::stream<Socket>> stream, boost::beast::flat_buffer* subscriptionBuffer, const std::unordered_map<std::string, std::function<void(const boost::system::error_code&, rapidjson::Value&&)>>& onReadHandlers, boost::mutex* mutex, rapidjson::Document::AllocatorType& allocator)
{
    stream->async_read(*subscriptionBuffer, [stream, subscriptionBuffer, &onReadHandlers, mutex, &allocator](const boost::system::error_code& errorCode, std::size_t bytesTransferred){
        boost::mutex::scoped_lock lock(*mutex);
        
        if (errorCode) {
            // invoke all callback functions with the error code
            for (std::unordered_map<std::string, std::function<void(const boost::system::error_code&, rapidjson::Value&&)>>::const_iterator it = onReadHandlers.cbegin(); it != onReadHandlers.cend(); ++it) {
                if (it->second) {
                    (it->second)(errorCode, rapidjson::Value());
                }
            }
            return;
        }

        std::string message = boost::beast::buffers_to_string(subscriptionBuffer->data());
        subscriptionBuffer->clear();

        // ignore pong message
        if (message.substr(0, 15) == std::string(R"({"type":"pong"})")) {
            // start the next asynchronous read
            _ReadFromSubscriptionStream(stream, subscriptionBuffer, onReadHandlers, mutex, allocator);
            return;
        }

        // parse the result
        rapidjson::Value result;
        if ( message.length() > 0 ) {
            std::stringstream stringStream(message);
            ParseJson(result, allocator, stringStream);
        }

        // find the callback function according to subscription id
        if (result.IsObject() && result.HasMember("id") && result["id"].IsString()) {
            std::string subscriptionId = result["id"].GetString();

            std::unordered_map<std::string, std::function<void(const boost::system::error_code&, rapidjson::Value&&)>>::const_iterator it = onReadHandlers.find(subscriptionId);
            if (it != onReadHandlers.end() && it->second) {
                // invoke callback function if there are payloads
                if (result.HasMember("payload")) {
                    (it->second)(errorCode, std::move(result["payload"]));
                }
            }
        }

        // start the next asynchronous read
        _ReadFromSubscriptionStream(stream, subscriptionBuffer, onReadHandlers, mutex, allocator);
    });
}

GraphSubscriptionWebSocketHandler::GraphSubscriptionWebSocketHandler(const ControllerClientInfo& clientInfo)
: _allocator(_allocatorBuffer, sizeof(_allocatorBuffer))
{
    boost::shared_ptr<boost::asio::io_context> ioContext = boost::make_shared<boost::asio::io_context>();
    std::string host = "localhost";
    uint16_t port = 80;
    if (clientInfo.unixEndpoint.empty()) {
        // use tcp socket
        host = clientInfo.host;
        port = clientInfo.httpPort == 0 ? 80 : clientInfo.httpPort;
        MUJIN_LOG_INFO(boost::format("Create TCP socket connected to host %s") % host);
        
        boost::asio::ip::tcp::socket socket(*ioContext);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);
        socket.connect(endpoint);
        _tcpStream = boost::make_shared<boost::beast::websocket::stream<boost::asio::ip::tcp::socket>>(std::move(socket));
    } else {
        // use unix domain socket
        MUJIN_LOG_INFO(boost::format("Create unix domain socket connected to endpoint %s") % clientInfo.unixEndpoint);

        boost::asio::local::stream_protocol::socket socket(*ioContext);
        boost::asio::local::stream_protocol::endpoint endpoint(clientInfo.unixEndpoint);
        socket.connect(endpoint);
        _unixSocketStream = boost::make_shared<boost::beast::websocket::stream<boost::asio::local::stream_protocol::socket>>(std::move(socket));
    }

    // upgrade the connection to websocket
    if (_tcpStream) {
        _tcpStream->handshake(host + ":" + std::to_string(port), "/api/v2/graphql");
    }
    if (_unixSocketStream) {
        _unixSocketStream->handshake(host + ":" + std::to_string(port), "/api/v2/graphql");
    }

    // encode username and password
    std::string usernamePassword = clientInfo.username + ":" + clientInfo.password;
    std::string encodedUsernamePassword;
    encodedUsernamePassword.resize(boost::beast::detail::base64::encoded_size(usernamePassword.size()));
    boost::beast::detail::base64::encode(&encodedUsernamePassword[0], usernamePassword.data(), usernamePassword.size());
    
    // add basic authorization header
    std::string connectionInitializationMessage = boost::str(boost::format(R"({"type":"connection_init","payload":{"Authorization":"Basic %s"}})") % encodedUsernamePassword);
    
    // initialize the websocket
    this->_SendMessage(connectionInitializationMessage);

    // read connection_ack from server
    if (_tcpStream) {
        _tcpStream->read(_subscriptionBuffer);
    }
    if (_unixSocketStream) {
        _unixSocketStream->read(_subscriptionBuffer);
    }

    std::string message = boost::beast::buffers_to_string(_subscriptionBuffer.data());
    if (message.find("connection_ack") == std::string::npos) {
        throw MUJIN_EXCEPTION_FORMAT("Failed to initialize websocket connection, Expected 'connection_ack' in response, but got: %s", message, MEC_HTTPServer);
    }
    _subscriptionBuffer.clear();

    // start the asynchronous read
    if (_tcpStream) {
        _ReadFromSubscriptionStream(_tcpStream, &_subscriptionBuffer, _onReadHandlers, &_mutex, _allocator);
    }
    if (_unixSocketStream) {
        _ReadFromSubscriptionStream(_unixSocketStream, &_subscriptionBuffer, _onReadHandlers, &_mutex, _allocator);
    }

    // start a new thread running I/O service until the socket is closed
    _thread = boost::make_shared<std::thread>([ioContext] {
        ioContext->run();
    });
}

bool GraphSubscriptionWebSocketHandler::IsStreamOpen()
{   
    boost::mutex::scoped_lock lock(_mutex);
    if (_tcpStream) {
        return _tcpStream->is_open();
    }
    if (_unixSocketStream) {
        return _unixSocketStream->is_open();
    }
}

std::string GraphSubscriptionWebSocketHandler::StartSubscription(const std::string& operationName, const std::string& query, const rapidjson::Value& rVariables, std::function<void(const boost::system::error_code&, rapidjson::Value&&)> onReadHandler)
{
    boost::mutex::scoped_lock lock(_mutex);

    // generate a random id for the subsctiption
    std::string subscriptionId = boost::uuids::to_string(_randomGenerator());
    MUJIN_LOG_INFO(boost::format("subscription %s started") % subscriptionId);

    // build the query
    rapidjson::Value payload;
    payload.SetObject();
    payload.AddMember(rapidjson::Document::StringRefType("operationName"), rapidjson::Value(operationName.c_str(), _allocator), _allocator);
    payload.AddMember(rapidjson::Document::StringRefType("query"), rapidjson::Value(query.c_str(), _allocator), _allocator);
    payload.AddMember(rapidjson::Document::StringRefType("variables"), rapidjson::Value(rVariables, _allocator), _allocator);

    rapidjson::Value request;
    request.SetObject();
    request.AddMember(rapidjson::Document::StringRefType("type"), "start", _allocator);
    request.AddMember(rapidjson::Document::StringRefType("payload"), payload, _allocator);
    request.AddMember(rapidjson::Document::StringRefType("id"), rapidjson::Value(subscriptionId.c_str(), _allocator), _allocator);
    
    _subscriptionStringBufferCache.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(_subscriptionStringBufferCache);
    request.Accept(writer);
    std::string subscriptionMessage = _subscriptionStringBufferCache.GetString();
    _subscriptionStringBufferCache.Clear();

    // save the callback function
    _onReadHandlers[subscriptionId] = onReadHandler;

    // start subscription
    this->_SendMessage(subscriptionMessage);

    return subscriptionId;
}

void GraphSubscriptionWebSocketHandler::StopSubscription(const std::string& subscriptionId)
{
    MUJIN_LOG_INFO(boost::format("subscription %s stopped") % subscriptionId);
    boost::mutex::scoped_lock lock(_mutex);

    // remove callback function
    std::unordered_map<std::string, std::function<void(const boost::system::error_code&, rapidjson::Value&&)>>::const_iterator it = _onReadHandlers.find(subscriptionId);
    if (it == _onReadHandlers.end()) {
        return;
    }
    _onReadHandlers.erase(it);

    try {
        // send subsciption completion message
        std::string completeMessage = boost::str(boost::format(R"({"id":"%s","type":"stop"})") % subscriptionId);
        this->_SendMessage(completeMessage);
    } catch (const std::exception& ex) {
        MUJIN_LOG_INFO(boost::format("failed to complete the subscription: %s") % ex.what());
    }
}

GraphSubscriptionWebSocketHandler::~GraphSubscriptionWebSocketHandler()
{
    try {
        // gracefully close the stream
        if (_tcpStream) {
            _tcpStream->close(boost::beast::websocket::close_code::normal);
        }
        if (_unixSocketStream) {
            _unixSocketStream->close(boost::beast::websocket::close_code::normal);
        }
    } catch (const std::exception& ex) {
        MUJIN_LOG_INFO(boost::format("failed to close the stream: %s") % ex.what());
    }

    _thread->join();
}

void GraphSubscriptionWebSocketHandler::_SendMessage(const std::string& message)
{
    if (_tcpStream) {
        _tcpStream->write(boost::asio::buffer(message));
    }
    if (_unixSocketStream) {
        _unixSocketStream->write(boost::asio::buffer(message));
    }
}

} // end namespace mujinclient
