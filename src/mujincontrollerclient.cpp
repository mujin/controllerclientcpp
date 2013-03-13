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
#define WIN32_LEAN_AND_MEAN
#include <mujincontrollerclient/mujincontrollerclient.h>

#include <boost/thread/mutex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <sstream>
#include <iostream>
#include <curl/curl.h>

#ifdef _MSC_VER
#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCDNAME__
#endif
#endif

#define GETCONTROLLERIMPL() ControllerClientImplPtr controller = boost::dynamic_pointer_cast<ControllerClientImpl>(GetController());
#define CHECKCURLCODE(code, msg) if (code != CURLE_OK) { \
        throw MujinException(str(boost::format("[%s:%d] curl function failed with error '%s': %s")%(__PRETTY_FUNCTION__)%(__LINE__)%curl_easy_strerror(code)%(msg)), MEC_HTTPClient); \
}

#define MUJIN_EXCEPTION_FORMAT0(s, errorcode) mujinclient::MujinException(boost::str(boost::format("[%s:%d] " s)%(__PRETTY_FUNCTION__)%(__LINE__)),errorcode)

/// adds the function name and line number to an exception
#define MUJIN_EXCEPTION_FORMAT(s, args,errorcode) mujinclient::MujinException(boost::str(boost::format("[%s:%d] " s)%(__PRETTY_FUNCTION__)%(__LINE__)%args),errorcode)

#define MUJIN_ASSERT_FORMAT(testexpr, s, args, errorcode) { if( !(testexpr) ) { throw mujinclient::MujinException(boost::str(boost::format("[%s:%d] (%s) failed " s)%(__PRETTY_FUNCTION__)%(__LINE__)%(# testexpr)%args),errorcode); } }

#define MUJIN_ASSERT_FORMAT0(testexpr, s, errorcode) { if( !(testexpr) ) { throw mujinclient::MujinException(boost::str(boost::format("[%s:%d] (%s) failed " s)%(__PRETTY_FUNCTION__)%(__LINE__)%(# testexpr)),errorcode); } }

// note that expr1 and expr2 will be evaluated twice if not equal
#define MUJIN_ASSERT_OP_FORMAT(expr1,op,expr2,s, args, errorcode) { if( !((expr1) op (expr2)) ) { throw mujinclient::MujinException(boost::str(boost::format("[%s:%d] %s %s %s, (eval %s %s %s) " s)%(__PRETTY_FUNCTION__)%(__LINE__)%(# expr1)%(# op)%(# expr2)%(expr1)%(# op)%(expr2)%args),errorcode); } }

#define MUJIN_ASSERT_OP_FORMAT0(expr1,op,expr2,s, errorcode) { if( !((expr1) op (expr2)) ) { throw mujinclient::MujinException(boost::str(boost::format("[%s:%d] %s %s %s, (eval %s %s %s) " s)%(__PRETTY_FUNCTION__)%(__LINE__)%(# expr1)%(# op)%(# expr2)%(expr1)%(# op)%(expr2)),errorcode); } }

#define MUJIN_ASSERT_OP(expr1,op,expr2) { if( !((expr1) op (expr2)) ) { throw mujinclient::MujinException(boost::str(boost::format("[%s:%d] %s %s %s, (eval %s %s %s) ")%(__PRETTY_FUNCTION__)%(__LINE__)%(# expr1)%(# op)%(# expr2)%(expr1)%(# op)%(expr2)),MEC_Assert); } }

namespace mujinclient {

static bool PairStringLengthCompare(const std::pair<std::string, std::string>&p0, const std::pair<std::string, std::string>&p1)
{
    return p0.first.size() > p1.first.size();
}

static std::string& SearchAndReplace(std::string& out, const std::string& in, const std::vector< std::pair<std::string, std::string> >&_pairs)
{
    BOOST_ASSERT(&out != &in);
    std::vector< std::pair<std::string, std::string> >::const_iterator itp, itbestp;
    for(itp = _pairs.begin(); itp != _pairs.end(); ++itp) {
        BOOST_ASSERT(itp->first.size()>0);
    }
    std::vector< std::pair<std::string, std::string> > pairs = _pairs;
    stable_sort(pairs.begin(),pairs.end(),PairStringLengthCompare);
    out.resize(0);
    size_t startindex = 0;
    while(startindex < in.size()) {
        size_t nextindex=std::string::npos;
        for(itp = pairs.begin(); itp != pairs.end(); ++itp) {
            size_t index = in.find(itp->first,startindex);
            if((nextindex == std::string::npos)|| ((index != std::string::npos)&&(index < nextindex)) ) {
                nextindex = index;
                itbestp = itp;
            }
        }
        if( nextindex == std::string::npos ) {
            out += in.substr(startindex);
            break;
        }
        out += in.substr(startindex,nextindex-startindex);
        out += itbestp->second;
        startindex = nextindex+itbestp->first.size();
    }
    return out;
}

#define SKIP_PEER_VERIFICATION // temporary
//#define SKIP_HOSTNAME_VERIFICATION

class ControllerClientImpl : public ControllerClient, public boost::enable_shared_from_this<ControllerClientImpl>
{
public:
    ControllerClientImpl(const std::string& usernamepassword, const std::string& baseuri, const std::string& proxyserverport, const std::string& proxyuserpw, int options)
    {
        _httpheaders = NULL;
        if( baseuri.size() > 0 ) {
            _baseuri = baseuri;
        }
        else {
            // use the default
            _baseuri = "https://controller.mujin.co.jp/";
        }
        _baseapiuri = _baseuri + std::string("api/v1/");
        //CURLcode code = curl_global_init(CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32);
        _curl = curl_easy_init();
        BOOST_ASSERT(!!_curl);

#ifdef _DEBUG
        curl_easy_setopt(_curl, CURLOPT_VERBOSE, 1L);
#endif

        CURLcode res;
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
        curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0l);
#endif

#ifdef SKIP_HOSTNAME_VERIFICATION
        /*
         * If the site you're connecting to uses a different host name that what
         * they have mentioned in their server certificate's commonName (or
         * subjectAltName) fields, libcurl will refuse to connect. You can skip
         * this check, but this will make the connection less secure.
         */
        curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

        SetProxy(proxyserverport, proxyuserpw);

        res = curl_easy_setopt(_curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        CHECKCURLCODE(res, "failed to set auth");
        res = curl_easy_setopt(_curl, CURLOPT_USERPWD, usernamepassword.c_str());
        CHECKCURLCODE(res, "failed to set userpw");

        // need to set the following?
        //CURLOPT_USERAGENT
        //CURLOPT_TCP_KEEPIDLE
        //CURLOPT_TCP_KEEPALIVE
        //CURLOPT_TCP_KEEPINTVL

        curl_easy_setopt(_curl, CURLOPT_COOKIEFILE, ""); // just to start the cookie engine

        // save everything to _buffer, neceesary to do it before first POST/GET calls or data will be output to stdout
        res = curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, _writer);
        CHECKCURLCODE(res, "failed to set writer");
        res = curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &_buffer);
        CHECKCURLCODE(res, "failed to set write data");

        std::string useragent = std::string("controllerclientcpp/")+MUJINCLIENT_VERSION_STRING;
        res = curl_easy_setopt(_curl, CURLOPT_USERAGENT, useragent.c_str());
        CHECKCURLCODE(res, "failed to set user-agent");

        res = curl_easy_setopt(_curl, CURLOPT_FOLLOWLOCATION, 0); // do not bounce through pages since we need to detect when login sessions expired
        CHECKCURLCODE(res, "failed to set follow location");
        res = curl_easy_setopt(_curl, CURLOPT_MAXREDIRS, 10);
        CHECKCURLCODE(res, "failed to max redirs");

        if( !(options & 1) ) {
            size_t index = usernamepassword.find_first_of(':');
            BOOST_ASSERT(index != std::string::npos );

            // make an initial GET call to get the CSRF token
            std::string loginuri = _baseuri + "login/";
            curl_easy_setopt(_curl, CURLOPT_URL, loginuri.c_str());
            curl_easy_setopt(_curl, CURLOPT_HTTPGET, 1);
            CURLcode res = curl_easy_perform(_curl);
            CHECKCURLCODE(res, "curl_easy_perform failed");
            long http_code = 0;
            res=curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
            CHECKCURLCODE(res, "curl_easy_getinfo");
            if( http_code == 302 ) {
                // most likely apache2-only authentication and login page isn't needed, however need to send another GET for the csrftoken
                loginuri = _baseuri + "api/v1/"; // pick some neutral page that is easy to load
                curl_easy_setopt(_curl, CURLOPT_URL, loginuri.c_str());
                CURLcode res = curl_easy_perform(_curl);
                CHECKCURLCODE(res, "curl_easy_perform failed");
                long http_code = 0;
                res = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
                CHECKCURLCODE(res, "curl_easy_getinfo");
                if( http_code != 200 ) {
                    throw MUJIN_EXCEPTION_FORMAT("HTTP GET %s returned HTTP error code %s", loginuri%http_code, MEC_HTTPServer);
                }
                _csrfmiddlewaretoken = _GetCSRFFromCookies();
                curl_easy_setopt(_curl, CURLOPT_REFERER, loginuri.c_str()); // necessary for SSL to work
            }
            else if( http_code == 200 ) {
                _csrfmiddlewaretoken = _GetCSRFFromCookies();
                std::string data = str(boost::format("username=%s&password=%s&this_is_the_login_form=1&next=%%2F&csrfmiddlewaretoken=%s")%usernamepassword.substr(0,index)%usernamepassword.substr(index+1)%_csrfmiddlewaretoken);
                curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE, data.size());
                curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, data.c_str());
                curl_easy_setopt(_curl, CURLOPT_REFERER, loginuri.c_str());
                //std::cout << "---performing post---" << std::endl;
                res = curl_easy_perform(_curl);
                CHECKCURLCODE(res, "curl_easy_perform failed");
                http_code = 0;
                res = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
                CHECKCURLCODE(res, "curl_easy_getinfo failed");
                if( http_code != 200 && http_code != 302 ) {
                    throw MUJIN_EXCEPTION_FORMAT("User login failed. HTTP POST %s returned HTTP status %s", loginuri%http_code, MEC_UserAuthentication);
                }
            }
            else {
                throw MUJIN_EXCEPTION_FORMAT("HTTP GET %s returned HTTP error code %s", loginuri%http_code, MEC_HTTPServer);
            }
        }

        _charset = "utf-8";
        _language = "en-us";
        _SetHTTPHeaders();

        try {
            GetProfile();
        }
        catch(const MujinException&) {
            // most likely username or password are
            throw MujinException(str(boost::format("failed to get controller profile, check username/password or if controller is active at %s")%_baseuri), MEC_UserAuthentication);
        }
    }

    std::string GetVersion()
    {
        return _profile.get<std::string>("version");
    }

    virtual ~ControllerClientImpl() {
        if( !!_httpheaders ) {
            curl_slist_free_all(_httpheaders);
        }
        curl_easy_cleanup(_curl);
    }

    virtual void SetCharacterEncoding(const std::string& newencoding)
    {
        boost::mutex::scoped_lock lock(_mutex);
        _charset = newencoding;
        _SetHTTPHeaders();
    }

    virtual void SetProxy(const std::string& serverport, const std::string& userpw)
    {
        curl_easy_setopt(_curl, CURLOPT_PROXY, serverport.c_str());
        curl_easy_setopt(_curl, CURLOPT_PROXYUSERPWD, userpw.c_str());
    }

    virtual void SetLanguage(const std::string& language)
    {
        boost::mutex::scoped_lock lock(_mutex);
        _language = language;
        _SetHTTPHeaders();
    }

    virtual void RestartServer()
    {
        boost::mutex::scoped_lock lock(_mutex);
        _uri = _baseuri + std::string("ajax/restartserver/");
        curl_easy_setopt(_curl, CURLOPT_URL, _uri.c_str());
        curl_easy_setopt(_curl, CURLOPT_POST, 1);
        curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE, 0);
        curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, NULL);
        CURLcode res = curl_easy_perform(_curl);
        CHECKCURLCODE(res, "curl_easy_perform failed");
        long http_code = 0;
        res = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
        CHECKCURLCODE(res, "curl_easy_getinfo failed");
        if( http_code != 200 ) {
            throw MUJIN_EXCEPTION_FORMAT0("Failed to restart server, please try again or contact MUJIN support", MEC_HTTPServer);
        }
    }

    virtual void Upgrade(const std::vector<unsigned char>& vdata)
    {
        BOOST_ASSERT(vdata.size()>0);
        boost::mutex::scoped_lock lock(_mutex);
        _uri = _baseuri + std::string("upgrade/");
        curl_easy_setopt(_curl, CURLOPT_URL, _uri.c_str());
        curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE, NULL);
        curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, NULL);

        // set new headers and remove the Expect: 100-continue
        struct curl_slist *headerlist=NULL;
        headerlist = curl_slist_append(headerlist, "Expect:");
        std::string s = std::string("X-CSRFToken: ")+_csrfmiddlewaretoken;
        headerlist = curl_slist_append(headerlist, s.c_str());
        curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, headerlist);

        // Fill in the file upload field
        struct curl_httppost *formpost=NULL, *lastptr=NULL;
        curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "file", CURLFORM_BUFFER, "mujinpatch", CURLFORM_BUFFERPTR, &vdata[0], CURLFORM_BUFFERLENGTH, vdata.size(), CURLFORM_END);
        curl_easy_setopt(_curl, CURLOPT_HTTPPOST, formpost);
        CURLcode res = curl_easy_perform(_curl);
        curl_formfree(formpost);
        // reset the headers before any exceptions are thrown
        _SetHTTPHeaders();
        CHECKCURLCODE(res, "curl_easy_perform failed");
        long http_code = 0;
        res = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
        CHECKCURLCODE(res, "curl_easy_getinfo failed");
        if( http_code != 200 ) {
            throw MUJIN_EXCEPTION_FORMAT0("Failed to upgrade server, please try again or contact MUJIN support", MEC_HTTPServer);
        }
    }

    virtual void CancelAllJobs()
    {
        CallDelete("job/?format=json");
    }

    virtual void GetScenePrimaryKeys(std::vector<std::string>& scenekeys)
    {
        boost::property_tree::ptree pt;
        CallGet("scene/?format=json&limit=0&fields=pk", pt);
        boost::property_tree::ptree& objects = pt.get_child("objects");
        scenekeys.resize(objects.size());
        size_t i = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
            scenekeys[i++] = v.second.get<std::string>("pk");
        }
    }

    virtual SceneResourcePtr ImportScene(const std::string& importuri, const std::string& importformat, const std::string& newuri)
    {
        boost::property_tree::ptree pt;
        CallPost("scene/?format=json&fields=pk", str(boost::format("{\"reference_uri\":\"%s\", \"reference_format\":\"%s\", \"uri\":\"%s\"}")%importuri%importformat%newuri), pt);
        std::string pk = pt.get<std::string>("pk");
        SceneResourcePtr scene(new SceneResource(shared_from_this(), pk));
        return scene;
    }

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallGet(const std::string& relativeuri, boost::property_tree::ptree& pt, int expectedhttpcode=200)
    {
        boost::mutex::scoped_lock lock(_mutex);
        _uri = _baseapiuri;
        _uri += relativeuri;
        curl_easy_setopt(_curl, CURLOPT_URL, _uri.c_str());
        _buffer.clear();
        _buffer.str("");
        curl_easy_setopt(_curl, CURLOPT_HTTPGET, 1);
        CURLcode res = curl_easy_perform(_curl);
        CHECKCURLCODE(res, "curl_easy_perform failed");
        long http_code = 0;
        res=curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
        CHECKCURLCODE(res, "curl_easy_getinfo");
        if( _buffer.rdbuf()->in_avail() > 0 ) {
            boost::property_tree::read_json(_buffer, pt);
        }
        if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
            std::string error_message = pt.get<std::string>("error_message", std::string());
            std::string traceback = pt.get<std::string>("traceback", std::string());
            throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s: %s", relativeuri%http_code%error_message, MEC_HTTPServer);
        }
        return http_code;
    }

    /// \brief expectedhttpcode is not 0, then will check with the returned http code and if not equal will throw an exception
    int CallPost(const std::string& relativeuri, const std::string& data, boost::property_tree::ptree& pt, int expectedhttpcode=201)
    {
        boost::mutex::scoped_lock lock(_mutex);
        _uri = _baseapiuri;
        _uri += relativeuri;
        curl_easy_setopt(_curl, CURLOPT_URL, _uri.c_str());
        _buffer.clear();
        _buffer.str("");
        curl_easy_setopt(_curl, CURLOPT_POST, 1);
        curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE, data.size());
        curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, data.size() > 0 ? data.c_str() : NULL);
        CURLcode res = curl_easy_perform(_curl);
        CHECKCURLCODE(res, "curl_easy_perform failed");
        long http_code = 0;
        res = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
        CHECKCURLCODE(res, "curl_easy_getinfo failed");
        if( _buffer.rdbuf()->in_avail() > 0 ) {
            boost::property_tree::read_json(_buffer, pt);
        }
        if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
            std::string error_message = pt.get<std::string>("error_message", std::string());
            std::string traceback = pt.get<std::string>("traceback", std::string());
            throw MUJIN_EXCEPTION_FORMAT("HTTP POST to '%s' returned HTTP status %s: %s", relativeuri%http_code%error_message, MEC_HTTPServer);
        }
        return http_code;
    }

    int CallPut(const std::string& relativeuri, const std::string& data, boost::property_tree::ptree& pt, int expectedhttpcode=202)
    {
        boost::mutex::scoped_lock lock(_mutex);
        _uri = _baseapiuri;
        _uri += relativeuri;
        curl_easy_setopt(_curl, CURLOPT_URL, _uri.c_str());
        _buffer.clear();
        _buffer.str("");
        curl_easy_setopt(_curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE, data.size());
        curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, data.size() > 0 ? data.c_str() : NULL);
        CURLcode res = curl_easy_perform(_curl);
        curl_easy_setopt(_curl, CURLOPT_CUSTOMREQUEST, NULL); // have to restore the default
        CHECKCURLCODE(res, "curl_easy_perform failed");
        long http_code = 0;
        res = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
        CHECKCURLCODE(res, "curl_easy_getinfo failed");
        if( _buffer.rdbuf()->in_avail() > 0 ) {
            boost::property_tree::read_json(_buffer, pt);
        }
        if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
            std::string error_message = pt.get<std::string>("error_message", std::string());
            std::string traceback = pt.get<std::string>("traceback", std::string());
            throw MUJIN_EXCEPTION_FORMAT("HTTP POST to '%s' returned HTTP status %s: %s", relativeuri%http_code%error_message, MEC_HTTPServer);
        }
        return http_code;
    }

    void CallDelete(const std::string& relativeuri)
    {
        boost::mutex::scoped_lock lock(_mutex);
        _uri = _baseapiuri;
        _uri += relativeuri;
        curl_easy_setopt(_curl, CURLOPT_URL, _uri.c_str());
        curl_easy_setopt(_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        CURLcode res = curl_easy_perform(_curl);
        curl_easy_setopt(_curl, CURLOPT_CUSTOMREQUEST, NULL); // have to restore the default
        CHECKCURLCODE(res, "curl_easy_perform failed");
        long http_code = 0;
        res = curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
        CHECKCURLCODE(res, "curl_easy_getinfo failed");
        if( http_code != 204 ) { // or 200 or 202 or 201?
            throw MUJIN_EXCEPTION_FORMAT("HTTP DELETE to '%s' returned HTTP status %s", relativeuri%http_code, MEC_HTTPServer);
        }
    }

    std::stringstream& GetBuffer()
    {
        return _buffer;
    }

    /* webdav operations
       const char *postdata =
       "<?xml version=\"1.0\"?><D:searchrequest xmlns:D=\"DAV:\" >"
       "<D:sql>SELECT \"http://schemas.microsoft.com/repl/contenttag\""
       " from SCOPE ('deep traversal of \"/exchange/adb/Calendar/\"') "
       "WHERE \"DAV:isfolder\" = True</D:sql></D:searchrequest>\r\n";

       CURLOPT_UPLOAD

     */

protected:

    void GetProfile()
    {
        _profile.clear();
        CallGet("profile/", _profile);
    }

    static int _writer(char *data, size_t size, size_t nmemb, std::stringstream *writerData)
    {
        if (writerData == NULL) {
            return 0;
        }
        writerData->write(data, size*nmemb);
        return size * nmemb;
    }

    void _SetHTTPHeaders()
    {
        // set the header to only send json
        std::string s = std::string("Content-Type: application/json; charset=") + _charset;
        if( !!_httpheaders ) {
            curl_slist_free_all(_httpheaders);
        }
        _httpheaders = curl_slist_append(NULL, s.c_str());
        s = str(boost::format("Accept-Language: %s,en-us")%_language);
        _httpheaders = curl_slist_append(_httpheaders, s.c_str()); //,en;q=0.7,ja;q=0.3',")
        //_httpheaders = curl_slist_append(_httpheaders, "Accept:"); // necessary?
        s = std::string("X-CSRFToken: ")+_csrfmiddlewaretoken;
        _httpheaders = curl_slist_append(_httpheaders, s.c_str());
        _httpheaders = curl_slist_append(_httpheaders, "Connection: Keep-Alive");
        _httpheaders = curl_slist_append(_httpheaders, "Keep-Alive: 20"); // keep alive for 20s?
        // test on windows first
        //_httpheaders = curl_slist_append(_httpheaders, "Accept-Encoding: gzip, deflate");
        curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _httpheaders);
    }

    std::string _GetCSRFFromCookies() {
        struct curl_slist *cookies;
        CURLcode res = curl_easy_getinfo(_curl, CURLINFO_COOKIELIST, &cookies);
        CHECKCURLCODE(res, "curl_easy_getinfo CURLINFO_COOKIELIST");
        struct curl_slist *nc = cookies;
        int i = 1;
        std::string csrfmiddlewaretoken;
        while (nc) {
            //std::cout << str(boost::format("[%d]: %s")%i%nc->data) << std::endl;
            char* csrftokenstart = strstr(nc->data, "csrftoken");
            if( !!csrftokenstart ) {
                std::stringstream ss(csrftokenstart+10);
                ss >> csrfmiddlewaretoken;
            }
            nc = nc->next;
            i++;
        }
        curl_slist_free_all(cookies);
        return csrfmiddlewaretoken;
    }

    int _lastmode;
    CURL *_curl;
    boost::mutex _mutex;
    std::stringstream _buffer;
    std::string _baseuri, _baseapiuri, _uri;

    curl_slist *_httpheaders;
    std::string _charset, _language;
    std::string _csrfmiddlewaretoken;

    boost::property_tree::ptree _profile; ///< user profile and versioning
};

typedef boost::shared_ptr<ControllerClientImpl> ControllerClientImplPtr;

WebResource::WebResource(ControllerClientPtr controller, const std::string& resourcename, const std::string& pk) : __controller(controller), __resourcename(resourcename), __pk(pk)
{
    BOOST_ASSERT(__pk.size()>0);
}

std::string WebResource::Get(const std::string& field)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("%s/%s/?format=json&fields=%s")%GetResourceName()%GetPrimaryKey()%field), pt);
    std::string fieldvalue = pt.get<std::string>(field);
    return fieldvalue;
}

void WebResource::Set(const std::string& field, const std::string& newvalue)
{
    throw MujinException("not implemented");
}

void WebResource::Delete()
{
    GETCONTROLLERIMPL();
    controller->CallDelete(str(boost::format("%s/%s/")%GetResourceName()%GetPrimaryKey()));
}

void WebResource::Copy(const std::string& newname, int options)
{
    throw MujinException("not implemented yet");
}

SceneResource::InstObject::InstObject(ControllerClientPtr controller, const std::string& scenepk, const std::string& pk) : WebResource(controller, str(boost::format("scene/%s/instobject")%scenepk), pk)
{
}

SceneResource::SceneResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller, "scene", pk)
{
}

TaskResourcePtr SceneResource::GetOrCreateTaskFromName(const std::string& taskname, const std::string& tasktype)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("scene/%s/task/?format=json&limit=1&name=%s&fields=pk,tasktype")%GetPrimaryKey()%taskname), pt);
    // task exists
    boost::property_tree::ptree& objects = pt.get_child("objects");
    if( objects.size() > 0 ) {
        std::string pk = objects.begin()->second.get<std::string>("pk");
        std::string currenttasktype = objects.begin()->second.get<std::string>("tasktype");
        if( currenttasktype != tasktype ) {
            throw MUJIN_EXCEPTION_FORMAT("task pk %s exists and has type %s, expected is %s", pk%currenttasktype%tasktype, MEC_InvalidState);
        }
        TaskResourcePtr task(new TaskResource(GetController(), pk));
        return task;
    }

    pt.clear();
    controller->CallPost(str(boost::format("scene/%s/task/?format=json&fields=pk")%GetPrimaryKey()), str(boost::format("{\"name\":\"%s\", \"tasktype\":\"%s\", \"scenepk\":\"%s\"}")%taskname%tasktype%GetPrimaryKey()), pt);
    std::string pk = pt.get<std::string>("pk");
    TaskResourcePtr task(new TaskResource(GetController(), pk));
    return task;
}

void SceneResource::GetTaskPrimaryKeys(std::vector<std::string>& taskkeys)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("scene/%s/task/?format=json&limit=0&fields=pk")%GetPrimaryKey()), pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    taskkeys.resize(objects.size());
    size_t i = 0;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
        taskkeys[i++] = v.second.get<std::string>("pk");
    }
}

void SceneResource::GetInstObjects(std::vector<SceneResource::InstObjectPtr>& instobjects)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("scene/%s/instobject/?format=json&limit=0&fields=instobjects")%GetPrimaryKey()), pt);
    boost::property_tree::ptree& objects = pt.get_child("instobjects");
    instobjects.resize(objects.size());
    size_t i = 0;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
        InstObjectPtr instobject(new InstObject(controller, GetPrimaryKey(), v.second.get<std::string>("pk")));
        //instobject->dofvalues
        instobject->name = v.second.get<std::string>("name");
        instobject->object_pk = v.second.get<std::string>("object_pk");
        instobject->reference_uri = v.second.get<std::string>("reference_uri");

        boost::property_tree::ptree& jsondofvalues = v.second.get_child("dofvalues");
        instobject->dofvalues.resize(jsondofvalues.size());
        size_t idof = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vdof, jsondofvalues) {
            instobject->dofvalues[idof++] = boost::lexical_cast<Real>(vdof.second.data());
        }

        size_t irotate = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vrotate, v.second.get_child("rotate")) {
            BOOST_ASSERT( irotate < 4 );
            instobject->rotate[irotate++] = boost::lexical_cast<Real>(vrotate.second.data());
        }
        size_t itranslate = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vtranslate, v.second.get_child("translate")) {
            BOOST_ASSERT( itranslate < 3 );
            instobject->translate[itranslate++] = boost::lexical_cast<Real>(vtranslate.second.data());
        }

        instobjects[i++] = instobject;
    }
}

TaskResource::TaskResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller,"task",pk)
{
}

void TaskResource::Execute()
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallPost(str(boost::format("task/%s/")%GetPrimaryKey()), std::string(), pt, 200);
}

void TaskResource::GetRunTimeStatus(JobStatus& status)
{
    throw MujinException("not implemented yet");
}

OptimizationResourcePtr TaskResource::GetOrCreateOptimizationFromName(const std::string& optimizationname, const std::string& optimizationtype)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("task/%s/optimization/?format=json&limit=1&name=%s&fields=pk,optimizationtype")%GetPrimaryKey()%optimizationname), pt);
    // optimization exists
    boost::property_tree::ptree& objects = pt.get_child("objects");
    if( objects.size() > 0 ) {
        std::string pk = objects.begin()->second.get<std::string>("pk");
        std::string currentoptimizationtype = objects.begin()->second.get<std::string>("optimizationtype");
        if( currentoptimizationtype != optimizationtype ) {
            throw MUJIN_EXCEPTION_FORMAT("optimization pk %s exists and has type %s, expected is %s", pk%currentoptimizationtype%optimizationtype, MEC_InvalidState);
        }
        OptimizationResourcePtr optimization(new OptimizationResource(GetController(), pk));
        return optimization;
    }

    pt.clear();
    controller->CallPost(str(boost::format("task/%s/optimization/?format=json&fields=pk")%GetPrimaryKey()), str(boost::format("{\"name\":\"%s\", \"optimizationtype\":\"%s\", \"taskpk\":\"%s\"}")%optimizationname%optimizationtype%GetPrimaryKey()), pt);
    std::string pk = pt.get<std::string>("pk");
    OptimizationResourcePtr optimization(new OptimizationResource(GetController(), pk));
    return optimization;
}

void TaskResource::GetOptimizationPrimaryKeys(std::vector<std::string>& optimizationkeys)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("task/%s/optimization/?format=json&limit=0&fields=pk")%GetPrimaryKey()), pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    optimizationkeys.resize(objects.size());
    size_t i = 0;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
        optimizationkeys[i++] = v.second.get<std::string>("pk");
    }
}

void TaskResource::GetTaskInfo(ITLPlanningTaskInfo& taskinfo)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("task/%s/?format=json&fields=taskgoalxml&tasktype")%GetPrimaryKey()), pt);
    std::string tasktype = pt.get<std::string>("tasktype");
    if( tasktype != "itlplanning" ) {
        throw MUJIN_EXCEPTION_FORMAT("task %s is type %s, expected itlplanning", GetPrimaryKey()%tasktype, MEC_InvalidArguments);
    }
    std::stringstream sstrans(pt.get<std::string>("taskgoalxml"));
    boost::property_tree::ptree pttrans;
    boost::property_tree::read_xml(sstrans, pttrans);
    boost::property_tree::ptree& objects = pttrans.get_child("root");
    taskinfo.SetDefaults();
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
        if( v.first == "startfromcurrent" ) {
            taskinfo.startfromcurrent = v.second.data() == std::string("True");
        }
        else if( v.first == "returntostart" ) {
            taskinfo.returntostart = v.second.data() == std::string("True");
        }
        else if( v.first == "usevrc" ) {
            taskinfo.usevrc = v.second.data() == std::string("True");
        }
        else if( v.first == "unit" ) {
            taskinfo.unit = v.second.data();
        }
        else if( v.first == "outputtrajtype" ) {
            taskinfo.outputtrajtype = v.second.data();
        }
        else if( v.first == "optimizationvalue" ) {
            taskinfo.optimizationvalue = boost::lexical_cast<Real>(v.second.data());
        }
        else if( v.first == "program" ) {
            taskinfo.program = v.second.data();
        }
    }
}

void TaskResource::SetTaskInfo(const ITLPlanningTaskInfo& taskinfo)
{
    GETCONTROLLERIMPL();
    std::string startfromcurrent = taskinfo.startfromcurrent ? "True" : "False";
    std::string usevrc = taskinfo.usevrc ? "True" : "False";
    std::string returntostart = taskinfo.returntostart ? "True" : "False";

    // because program will inside string, encode newlines
    std::string program;
    std::vector< std::pair<std::string, std::string> > serachpairs(2);
    serachpairs[0].first = "\n"; serachpairs[0].second = "\\n";
    serachpairs[1].first = "\r\n"; serachpairs[1].second = "\\n";
    SearchAndReplace(program, taskinfo.program, serachpairs);
    std::string taskgoalput = str(boost::format("{\"taskgoalxml\":\"<root><outputtrajtype>%s</outputtrajtype><program>%s</program><unit>%s</unit><optimizationvalue>%f</optimizationvalue><startfromcurrent>%s</startfromcurrent><usevrc>%s</usevrc><returntostart>%s</returntostart></root>\"}")%taskinfo.outputtrajtype%program%taskinfo.unit%taskinfo.optimizationvalue%startfromcurrent%usevrc%returntostart);
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("task/%s/?format=json&fields=")%GetPrimaryKey()), taskgoalput, pt);
}

PlanningResultResourcePtr TaskResource::GetResult()
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("task/%s/result/?format=json&limit=1&optimization=None&fields=pk")%GetPrimaryKey()), pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    if( objects.size() == 0 ) {
        return PlanningResultResourcePtr();
    }
    std::string pk = objects.begin()->second.get<std::string>("pk");
    PlanningResultResourcePtr result(new PlanningResultResource(GetController(), pk));
    return result;
}

OptimizationResource::OptimizationResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller,"optimization",pk)
{
}

void OptimizationResource::Execute()
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallPost(str(boost::format("optimization/%s/")%GetPrimaryKey()), std::string(), pt, 200);
}

void OptimizationResource::GetRunTimeStatus(JobStatus& status) {
    throw MujinException("not implemented yet");
}

void OptimizationResource::SetOptimizationInfo(const RobotPlacementOptimizationInfo& optimizationinfo)
{
    GETCONTROLLERIMPL();
    std::string ignorebasecollision = optimizationinfo.ignorebasecollision ? "True" : "False";
    std::string optimizationgoalput = str(boost::format("{\"optimizationparametersxml\":\"<root><name>%s</name><maxstorecandidates>%d</maxstorecandidates><unit>%s</unit><ignorebasecollision>%s</ignorebasecollision><frame>%s</frame><maxrange_>%f</maxrange_><maxrange_>%f</maxrange_><maxrange_>%f</maxrange_><maxrange_>%f</maxrange_><minrange_>%f</minrange_><minrange_>%f</minrange_><minrange_>%f</minrange_><minrange_>%f</minrange_><stepsize_>%f</stepsize_><stepsize_>%f</stepsize_><stepsize_>%f</stepsize_><stepsize_>%f</stepsize_></root>\"}")%optimizationinfo.name%optimizationinfo.maxstorecandidates%optimizationinfo.unit%ignorebasecollision%optimizationinfo.frame%optimizationinfo.maxrange[0]%optimizationinfo.maxrange[1]%optimizationinfo.maxrange[2]%optimizationinfo.maxrange[3]%optimizationinfo.minrange[0]%optimizationinfo.minrange[1]%optimizationinfo.minrange[2]%optimizationinfo.minrange[3]%optimizationinfo.stepsize[0]%optimizationinfo.stepsize[1]%optimizationinfo.stepsize[2]%optimizationinfo.stepsize[3]);
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("optimization/%s/?format=json&fields=")%GetPrimaryKey()), optimizationgoalput, pt);
}

void OptimizationResource::GetResults(int fastestnum, std::vector<PlanningResultResourcePtr>& results)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("optimization/%s/result/?format=json&limit=%d&fields=pk&order_by=task_time")%GetPrimaryKey()%fastestnum), pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    results.resize(objects.size());
    size_t i = 0;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
        results[i++].reset(new PlanningResultResource(controller, v.second.get<std::string>("pk")));
    }
}

PlanningResultResource::PlanningResultResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller,"planningresult",pk)
{
}

void PlanningResultResource::GetTransforms(std::map<std::string, Transform>& transforms)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("%s/%s/?format=json&fields=transformxml")%GetResourceName()%GetPrimaryKey()), pt);
    std::stringstream sstrans(pt.get<std::string>("transformxml"));
    boost::property_tree::ptree pttrans;
    boost::property_tree::read_xml(sstrans, pttrans);
    boost::property_tree::ptree& objects = pttrans.get_child("root");
    transforms.clear();
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
        if( v.first == "transforms_" ) {
            Transform t;
            std::string name;
            int itranslation=0, iquat=0;
            BOOST_FOREACH(boost::property_tree::ptree::value_type &vtrans, v.second) {
                if( vtrans.first == "name" ) {
                    name = vtrans.second.data();
                }
                else if( vtrans.first == "translation_" && itranslation < 3 ) {
                    t.translation[itranslation++] = boost::lexical_cast<Real>(vtrans.second.data());
                }
                else if( vtrans.first == "quat_" && iquat < 4 ) {
                    t.quat[iquat++] = boost::lexical_cast<Real>(vtrans.second.data());
                }
            }
            // normalize the quaternion
            Real dist2 = t.quat[0]*t.quat[0] + t.quat[1]*t.quat[1] + t.quat[2]*t.quat[2] + t.quat[3]*t.quat[3];
            if( dist2 > 0 ) {
                Real fnorm =1/std::sqrt(dist2);
                t.quat[0] *= fnorm; t.quat[1] *= fnorm; t.quat[2] *= fnorm; t.quat[3] *= fnorm;
            }
            transforms[name] = t;
        }
    }
    //std::string fieldvalue = pt.get<std::string>(field);
    //return fieldvalue;
}

//    transformxml
ControllerClientPtr CreateControllerClient(const std::string& usernamepassword, const std::string& baseurl, const std::string& proxyserverport, const std::string& proxyuserpw, int options)
{
    return ControllerClientPtr(new ControllerClientImpl(usernamepassword, baseurl, proxyserverport, proxyuserpw, options));
}

void ControllerClientDestroy()
{
}

void ComputeMatrixFromTransform(Real matrix[12], const Transform &transform)
{
    throw MujinException("not implemented yet");
//    length2 = numpy.sum(quat**2)
//    ilength2 = 2.0/length2
//    qq1 = ilength2*quat[1]*quat[1]
//    qq2 = ilength2*quat[2]*quat[2]
//    qq3 = ilength2*quat[3]*quat[3]
//    T = numpy.eye(4)
//    T[0,0] = 1 - qq2 - qq3
//    T[0,1] = ilength2*(quat[1]*quat[2] - quat[0]*quat[3])
//    T[0,2] = ilength2*(quat[1]*quat[3] + quat[0]*quat[2])
//    T[1,0] = ilength2*(quat[1]*quat[2] + quat[0]*quat[3])
//    T[1,1] = 1 - qq1 - qq3
//    T[1,2] = ilength2*(quat[2]*quat[3] - quat[0]*quat[1])
//    T[2,0] = ilength2*(quat[1]*quat[3] - quat[0]*quat[2])
//    T[2,1] = ilength2*(quat[2]*quat[3] + quat[0]*quat[1])
//    T[2,2] = 1 - qq1 - qq2
}

void ComputeZXYFromMatrix(Real ZXY[3], Real matrix[12])
{
    throw MujinException("not implemented yet");
//    if abs(T[2][0]) < 1e-10 and abs(T[2][2]) < 1e-10:
//        sinx = T[2][1]
//        x = numpy.pi/2 if sinx > 0 else -numpy.pi/2
//        z = 0.0
//        y = numpy.arctan2(sinx*T[1][0],T[0][0])
//    else:
//        y = numpy.arctan2(-T[2][0],T[2][2])
//        siny = numpy.sin(y)
//        cosy = numpy.cos(y)
//        Ryinv = numpy.array([[cosy,0,-siny],[0,1,0],[siny,0,cosy]])
//        Rzx = numpy.dot(T[0:3,0:3],Ryinv)
//        x = numpy.arctan2(Rzx[2][1],Rzx[2][2])
//        z = numpy.arctan2(Rzx[1][0],Rzx[0][0])
//    return numpy.array([x,y,z])
}

void ComputeZXYFromTransform(Real ZXY[3], const Transform& transform)
{
    throw MujinException("not implemented yet");
    //zxyFromMatrix(matrixFromTransform())
}

}
