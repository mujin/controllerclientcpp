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
#else

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

#ifdef _MSC_VER
#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCDNAME__
#endif
#endif

#define GETCONTROLLERIMPL() ControllerClientImplPtr controller = boost::dynamic_pointer_cast<ControllerClientImpl>(GetController());
#define CHECKCURLCODE(code, msg) if (code != CURLE_OK) { \
        throw MujinException(str(boost::format("[%s:%d] curl function %s with error='%s': %s")%(__PRETTY_FUNCTION__)%(__LINE__)%(msg)%curl_easy_strerror(code)%_errormessage), MEC_HTTPClient); \
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

BOOST_STATIC_ASSERT(sizeof(unsigned short) == 2); // need this for utf-16 reading

namespace mujinclient {


namespace encoding {

#if defined(_WIN32) || defined(_WIN64)

// Build a table of the mapping between code pages and web charsets
// It's hack, but at least it doesn't drag in a bunch of unnecessary dependencies
// http://msdn.microsoft.com/en-us/library/aa288104%28v=vs.71%29.aspx
std::map<int, std::string> InitializeCodePageMap()
{
    std::map<int, std::string> mapCodePageToCharset;
    mapCodePageToCharset[866] = "IBM866";
    mapCodePageToCharset[852] = "IBM852";
    mapCodePageToCharset[949] = "KS_C_5601-1987";
    mapCodePageToCharset[50220 /*CODE_JPN_JIS*/] = "ISO-2022-JP";
    mapCodePageToCharset[874] = "windows-874";
    mapCodePageToCharset[20866] = "koi8-r";
    mapCodePageToCharset[1251] = "x-cp1251";
    mapCodePageToCharset[50225] = "ISO-2022-KR";
    mapCodePageToCharset[1256] = "windows-1256";
    mapCodePageToCharset[1257] = "windows-1257";
    mapCodePageToCharset[1254] = "windows-1254";
    mapCodePageToCharset[1255] = "windows-1255";
    mapCodePageToCharset[1252] = "windows-1252";
    mapCodePageToCharset[1253] = "windows-1253";
    mapCodePageToCharset[1250] = "x-cp1250";
    mapCodePageToCharset[950] = "x-x-big5";
    mapCodePageToCharset[932] = "Shift_JIS";
    mapCodePageToCharset[51932 /*CODE_JPN_EUC*/] = "EUC-JP";
    mapCodePageToCharset[28592] = "latin2";
    mapCodePageToCharset[936] = "ISO-IR-58";
    mapCodePageToCharset[1258] = "windows-1258";
    mapCodePageToCharset[65001] = "utf-8";
    return mapCodePageToCharset;
}
static std::map<int, std::string> s_mapCodePageToCharset = InitializeCodePageMap();

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
std::string ConvertUTF8ToFileSystemEncoding(const std::string& utf8)
{
#if defined(_WIN32) || defined(_WIN64)
    return encoding::ConvertUTF8toMBS(utf8);
#else
    // most linux systems use utf-8, can use getenv("LANG") to double-check
    return utf8;
#endif
}

/// \brief converts utf-8 encoded string into the encoding  string that the filesystem uses
std::string ConvertUTF16ToFileSystemEncoding(const std::wstring& utf16)
{
#if defined(_WIN32) || defined(_WIN64)
	return encoding::ConvertUTF16toMBS(utf16);
#else
	// most linux systems use utf-8, can use getenv("LANG") to double-check
    std::string utf8;
    utf8::utf16to8(utf8.begin(), utf16.end(), std::back_inserter(utf8));
	return utf8;
#endif
}

/// \brief converts utf-8 encoded string into the encoding  string that the filesystem uses
std::string ConvertFileSystemEncodingToUTF8(const std::string& fs)
{
#if defined(_WIN32) || defined(_WIN64)
    return encoding::ConvertMBStoUTF8(fs);
#else
    // most linux systems use utf-8, can use getenv("LANG") to double-check
    return fs;
#endif
}

}

#ifdef _WIN32
const char s_filesep = '\\';
const char s_wfilesep = L'\\';
#else
const char s_filesep = '/';
const char s_wfilesep = L'/';
#endif

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

template <typename T>
std::wstring ParseWincapsWCNPath(const T& sourcefilename, const boost::function<std::string(const T&)>& ConvertToFileSystemEncoding)
{
    // scenefilenames is the WPJ file, so have to open it up to see what directory it points to
    // note that the encoding is utf-16
    // <clsProject Object="True">
    //   <WCNPath VT="8">.\threegoaltouch\threegoaltouch.WCN;</WCNPath>
    // </clsProject>
    // first have to get the raw utf-16 data
    std::ifstream wpjfilestream(sourcefilename.c_str(), std::ios::binary|std::ios::in);
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

class FileHandler
{
public:
    FileHandler(const char* pfilename) {
        _fd = fopen(pfilename, "rb");
    }
    FileHandler(const wchar_t* pfilename) {
        _fd = _wfopen(pfilename, L"rb");
    }
    ~FileHandler() {
        if( !!_fd ) {
            fclose(_fd);
        }
    }
    FILE* _fd;
};

#define SKIP_PEER_VERIFICATION // temporary
//#define SKIP_HOSTNAME_VERIFICATION

class ControllerClientImpl : public ControllerClient, public boost::enable_shared_from_this<ControllerClientImpl>
{
public:
    ControllerClientImpl(const std::string& usernamepassword, const std::string& baseuri, const std::string& proxyserverport, const std::string& proxyuserpw, int options)
    {
        size_t usernameindex = usernamepassword.find_first_of(':');
        BOOST_ASSERT(usernameindex != std::string::npos );
        std::string username = usernamepassword.substr(0,usernameindex);
        std::string password = usernamepassword.substr(usernameindex+1);

        _httpheaders = NULL;
        if( baseuri.size() > 0 ) {
            _baseuri = baseuri;
            // ensure trailing slash
            if( _baseuri[_baseuri.size()-1] != '/' ) {
                _baseuri.push_back('/');
            }
        }
        else {
            // use the default
            _baseuri = "https://controller.mujin.co.jp/";
        }
        _baseapiuri = _baseuri + std::string("api/v1/");
        // hack for now since webdav server and api server could be running on different ports
        if( boost::algorithm::ends_with(_baseuri, ":8000/") ) {
            // testing on localhost, however the webdav server is running on port 80...
            _basewebdavuri = str(boost::format("%s/u/%s/")%_baseuri.substr(0,_baseuri.size()-6)%username);
        }
        else {
            _basewebdavuri = str(boost::format("%su/%s/")%_baseuri%username);
        }

        //CURLcode code = curl_global_init(CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32);
        _curl = curl_easy_init();
        BOOST_ASSERT(!!_curl);

#ifdef _DEBUG
        //curl_easy_setopt(_curl, CURLOPT_VERBOSE, 1L);
#endif
        _errormessage.resize(CURL_ERROR_SIZE);
        curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, &_errormessage[0]);

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

        if( proxyserverport.size() > 0 ) {
            SetProxy(proxyserverport, proxyuserpw);
        }

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
                std::string data = str(boost::format("username=%s&password=%s&this_is_the_login_form=1&next=%%2F&csrfmiddlewaretoken=%s")%username%password%_csrfmiddlewaretoken);
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
#if defined(_WIN32) || defined(_WIN64)
        UINT codepage = GetACP();
        if( encoding::s_mapCodePageToCharset.find(codepage) != encoding::s_mapCodePageToCharset.end() ) {
            _charset = encoding::s_mapCodePageToCharset[codepage];
        }
#endif
        std::cout << "setting character set to " << _charset << std::endl;
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

    virtual void GetRunTimeStatuses(std::vector<JobStatus>& statuses, int options)
    {
        boost::property_tree::ptree pt;
        std::string url = "job/?format=json&fields=pk,status,fnname,elapsedtime";
        if( options & 1 ) {
            url += std::string(",status_text");
        }
        CallGet(url, pt);
        boost::property_tree::ptree& objects = pt.get_child("objects");
        size_t i = 0;
        statuses.resize(objects.size());
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
            statuses[i].pk = v.second.get<std::string>("pk");
            statuses[i].code = static_cast<JobStatusCode>(boost::lexical_cast<int>(v.second.get<std::string>("status")));
            statuses[i].type = v.second.get<std::string>("fnname");
            statuses[i].elapsedtime = v.second.get<double>("elapsedtime");
            if( options & 1 ) {
                statuses[i].message = v.second.get<std::string>("status_text");
            }
            i++;
        }
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

    virtual SceneResourcePtr RegisterScene(const std::string& uri, const std::string& scenetype)
    {
        BOOST_ASSERT(scenetype.size()>0);
        boost::property_tree::ptree pt;
        CallPost("scene/?format=json&fields=pk", str(boost::format("{\"uri\":\"%s\", \"scenetype\":\"%s\"}")%uri%scenetype), pt);
        std::string pk = pt.get<std::string>("pk");
        SceneResourcePtr scene(new SceneResource(shared_from_this(), pk));
        return scene;
    }

    virtual SceneResourcePtr ImportSceneToCOLLADA(const std::string& importuri, const std::string& importformat, const std::string& newuri)
    {
        BOOST_ASSERT(importformat.size()>0);
        boost::property_tree::ptree pt;
        CallPost("scene/?format=json&fields=pk", str(boost::format("{\"reference_uri\":\"%s\", \"reference_format\":\"%s\", \"uri\":\"%s\"}")%importuri%importformat%newuri), pt);
        std::string pk = pt.get<std::string>("pk");
        SceneResourcePtr scene(new SceneResource(shared_from_this(), pk));
        return scene;
    }

    class CurlCustomRequestSetter
    {
public:
        CurlCustomRequestSetter(CURL *curl, const char* method) : _curl(curl) {
            curl_easy_setopt(_curl, CURLOPT_CUSTOMREQUEST, method);
        }
        ~CurlCustomRequestSetter() {
            curl_easy_setopt(_curl, CURLOPT_CUSTOMREQUEST, NULL);
        }
protected:
        CURL* _curl;
    };

    class CurlUploadSetter
    {
public:
        CurlUploadSetter(CURL *curl) : _curl(curl) {
            curl_easy_setopt(_curl, CURLOPT_UPLOAD, 1L);
        }
        ~CurlUploadSetter() {
            curl_easy_setopt(_curl, CURLOPT_UPLOAD, 0L);
        }
protected:
        CURL* _curl;
    };

    virtual void SyncUpload_UTF8(const std::string& sourcefilename, const std::string& destinationdir, const std::string& scenetype)
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
                _UploadDirectoryToWebDAV_UTF8(sCopyDir, baseuploaduri+_EncodeWithoutSeparator(strWCNURI.substr(0,lastindex)));
            }
        }

        // sourcefilenamebase is utf-8
        char* pescapeddir = curl_easy_escape(_curl, sourcefilename.substr(nBaseFilenameStartIndex).c_str(), 0);
        std::string uploadfileuri = baseuploaduri + std::string(pescapeddir);
        curl_free(pescapeddir);
        _UploadFileToWebDAV_UTF8(sourcefilename, uploadfileuri);

        /* webdav operations
           const char *postdata =
           "<?xml version=\"1.0\"?><D:searchrequest xmlns:D=\"DAV:\" >"
           "<D:sql>SELECT \"http://schemas.microsoft.com/repl/contenttag\""
           " from SCOPE ('deep traversal of \"/exchange/adb/Calendar/\"') "
           "WHERE \"DAV:isfolder\" = True</D:sql></D:searchrequest>\r\n";
         */
    }

    virtual void SyncUpload_UTF16(const std::wstring& sourcefilename_utf16, const std::wstring& destinationdir_utf16, const std::string& scenetype)
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
                _UploadDirectoryToWebDAV_UTF16(sCopyDir_utf16, baseuploaduri+_EncodeWithoutSeparator(strWCNURI.substr(0,lastindex_utf8)));
            }
        }

		// sourcefilenamebase is utf-8
        std::string sourcefilenamedir_utf8;
        utf8::utf16to8(sourcefilename_utf16.begin(), sourcefilename_utf16.begin()+nBaseFilenameStartIndex, std::back_inserter(sourcefilenamedir_utf8));
        char* pescapeddir = curl_easy_escape(_curl, sourcefilenamedir_utf8.c_str(), 0);
        std::string uploadfileuri = baseuploaduri + std::string(pescapeddir);
        curl_free(pescapeddir);
        //_UploadFileToWebDAV_UTF16(sourcefilename_utf16, uploadfileuri);
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
    int CallGet(const std::string& relativeuri, std::string& outputdata, int expectedhttpcode=200)
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
        outputdata = _buffer.str();
        if( expectedhttpcode != 0 && http_code != expectedhttpcode ) {
            if( outputdata.size() > 0 ) {
                boost::property_tree::ptree pt;
                boost::property_tree::read_json(_buffer, pt);
                std::string error_message = pt.get<std::string>("error_message", std::string());
                std::string traceback = pt.get<std::string>("traceback", std::string());
                throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s: %s", relativeuri%http_code%error_message, MEC_HTTPServer);
            }
            throw MUJIN_EXCEPTION_FORMAT("HTTP GET to '%s' returned HTTP status %s", relativeuri%http_code, MEC_HTTPServer);
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

    virtual void SetDefaultSceneType(const std::string& scenetype)
    {
        _defaultscenetype = scenetype;
    }

    virtual const std::string& GetDefaultSceneType()
    {
        return _defaultscenetype;
    }

    virtual void SetDefaultTaskType(const std::string& tasktype)
    {
        _defaultscenetype = tasktype;
    }

    virtual const std::string& GetDefaultTaskType()
    {
        return _defaulttasktype;
    }

    std::string GetScenePrimaryKeyFromURI_UTF8(const std::string& uri)
    {
        size_t index = uri.find(":/");
        MUJIN_ASSERT_OP_FORMAT(index,!=,std::string::npos, "bad URI: %s", uri, MEC_InvalidArguments);
        uri.substr(index+2);
        char* pcurlresult = curl_easy_escape(_curl, uri.c_str()+index+2,uri.size()-index-2);
        std::string sresult(pcurlresult);
        curl_free(pcurlresult); // have to release the result
        return sresult;
    }

    std::string GetScenePrimaryKeyFromURI_UTF16(const std::wstring& uri)
    {
        std::string utf8line;
        utf8::utf16to8(uri.begin(), uri.end(), std::back_inserter(utf8line));
        return GetScenePrimaryKeyFromURI_UTF8(utf8line);
    }

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

    // encode a URL without the / separator
    std::string _EncodeWithoutSeparator(const std::string& raw)
    {
        std::string output;
        size_t startindex = 0;
        for(size_t i = 0; i < raw.size(); ++i) {
            if( raw[i] == '/' ) {
                if( startindex != i ) {
                    char* pescaped = curl_easy_escape(_curl, raw.c_str()+startindex, i-startindex);
                    output += std::string(pescaped);
                    curl_free(pescaped);
                    startindex = i+1;
                }
                output += '/';
            }
        }
        if( startindex != raw.size() ) {
            char* pescaped = curl_easy_escape(_curl, raw.c_str()+startindex, raw.size()-startindex);
            output += std::string(pescaped);
            curl_free(pescaped);
        }
        return output;
    }

    /// \param destinationdir the directory inside the user webdav folder. has a trailing slash
    void _EnsureWebDAVDirectories(const std::string& uriDestinationDir)
    {
        std::list<std::string> listCreateDirs;
        std::string output;
        size_t startindex = 0;
        for(size_t i = 0; i < uriDestinationDir.size(); ++i) {
            if( uriDestinationDir[i] == '/' ) {
                if( startindex != i ) {
                    char* pescaped = curl_easy_escape(_curl, uriDestinationDir.c_str()+startindex, i-startindex);
                    listCreateDirs.push_back(std::string(pescaped));
                    curl_free(pescaped);
                    startindex = i+1;
                }
            }
        }
        if( startindex != uriDestinationDir.size() ) {
            char* pescaped = curl_easy_escape(_curl, uriDestinationDir.c_str()+startindex, uriDestinationDir.size()-startindex);
            listCreateDirs.push_back(std::string(pescaped));
            curl_free(pescaped);
        }

        // Check that the directory exists
        //curl_easy_setopt(_curl, CURLOPT_URL, buff);
        //curl_easy_setopt(_curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
        //res = curl_easy_perform(self->send_handle);
        //if(res != 0) {
        // // does not exist
        //}

        CurlCustomRequestSetter setter(_curl, "MKCOL");
        std::string totaluri = "";
        for(std::list<std::string>::iterator itdir = listCreateDirs.begin(); itdir != listCreateDirs.end(); ++itdir) {
            // first have to create the directory structure up to destinationdir
            if( totaluri.size() > 0 ) {
                totaluri += '/';
            }
            totaluri += *itdir;
            _uri = _basewebdavuri + totaluri;
            curl_easy_setopt(_curl, CURLOPT_URL, _uri.c_str());
            CURLcode res = curl_easy_perform(_curl);
            CHECKCURLCODE(res, "curl_easy_perform failed");
            long http_code = 0;
            res=curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
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

    /// \brief recursively uploads a directory and creates directories along the way if they don't exist
    ///
	/// overwrites all the files
	/// \param copydir is utf-8 encoded
	/// \param uri is URI-encoded
    void _UploadDirectoryToWebDAV_UTF8(const std::string& copydir, const std::string& uri)
    {
        {
            // make sure the directory is created
            CurlCustomRequestSetter setter(_curl, "MKCOL");
            curl_easy_setopt(_curl, CURLOPT_URL, uri.c_str());
            CURLcode res = curl_easy_perform(_curl);
            CHECKCURLCODE(res, "curl_easy_perform failed");
            long http_code = 0;
            res=curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
            if( http_code != 201 && http_code != 301 ) {
                throw MUJIN_EXCEPTION_FORMAT("HTTP MKCOL failed for %s with HTTP status %d: %s", uri%http_code%_errormessage, MEC_HTTPServer);
            }
        }

        std::string sCopyDir_FS = encoding::ConvertUTF8ToFileSystemEncoding(copydir);
        std::cout << "uploading " << sCopyDir_FS << " -> " << uri << std::endl;

#if defined(_WIN32) || defined(_WIN64)
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
                std::string newcopydir = str(boost::format("%s\\%s")%copydir%filename_utf8);
                char* pescapeddir = curl_easy_escape(_curl, filename_utf8.c_str(), filename_utf8.size());
                std::string newuri = str(boost::format("%s/%s")%uri%pescapeddir);
                curl_free(pescapeddir);

                if( ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
                    _UploadDirectoryToWebDAV_UTF8(newcopydir, newuri);
                }
                else if( ffd.dwFileAttributes == 0 || ffd.dwFileAttributes == FILE_ATTRIBUTE_READONLY || ffd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL || ffd.dwFileAttributes == FILE_ATTRIBUTE_ARCHIVE ) {
                    _UploadFileToWebDAV_UTF8(newcopydir, newuri);
                }
            }
        } while(FindNextFileA(hFind,&ffd) != 0);

        DWORD err = GetLastError();
        FindClose(hFind);
        if( err != ERROR_NO_MORE_FILES ) {
            throw MUJIN_EXCEPTION_FORMAT("system error 0x%x when recursing through %s", err%sCopyDir_FS, MEC_HTTPServer);
        }

#else
        boost::filesystem::path bfpcopydir(copydir);
        for(boost::filesystem::directory_iterator itdir(bfpcopydir); itdir != boost::filesystem::directory_iterator(); ++itdir) {
#if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
            std::string dirfilename = encoding::ConvertFileSystemEncodingToUTF8(itdir->path().filename().string());
#else
            std::string dirfilename = encoding::ConvertFileSystemEncodingToUTF8(itdir->path().filename());
#endif
            char* pescapeddir = curl_easy_escape(_curl, dirfilename.c_str(), dirfilename.size());
            std::string newuri = str(boost::format("%s/%s")%uri%pescapeddir);
            curl_free(pescapeddir);
            if( boost::filesystem::is_directory(itdir->status()) ) {
                _UploadDirectoryToWebDAV_UTF8(itdir->path().string(), newuri);
            }
            else if( boost::filesystem::is_regular_file(itdir->status()) ) {
                _UploadFileToWebDAV_UTF8(itdir->path().string(), newuri);
            }
        }
#endif // defined(_WIN32) || defined(_WIN64)
    }

    /// \brief recursively uploads a directory and creates directories along the way if they don't exist
    ///
	/// overwrites all the files
	/// \param copydir is utf-16 encoded
	/// \param uri is URI-encoded
    void _UploadDirectoryToWebDAV_UTF16(const std::wstring& copydir, const std::string& uri)
    {
        {
            // make sure the directory is created
            CurlCustomRequestSetter setter(_curl, "MKCOL");
            curl_easy_setopt(_curl, CURLOPT_URL, uri.c_str());
            CURLcode res = curl_easy_perform(_curl);
            CHECKCURLCODE(res, "curl_easy_perform failed");
            long http_code = 0;
            res=curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
            if( http_code != 201 && http_code != 301 ) {
                throw MUJIN_EXCEPTION_FORMAT("HTTP MKCOL failed for %s with HTTP status %d: %s", uri%http_code%_errormessage, MEC_HTTPServer);
            }
        }

        std::wstring sCopyDir_FS = copydir;
        std::cout << "uploading " << encoding::ConvertUTF16ToFileSystemEncoding(copydir) << " -> " << uri << std::endl;

#if defined(_WIN32) || defined(_WIN64)
        WIN32_FIND_DATAW ffd;
        std::wstring searchstr = sCopyDir_FS + std::wstring(L"\\*");
        HANDLE hFind = FindFirstFileW(searchstr.c_str(), &ffd);
        if (hFind == INVALID_HANDLE_VALUE)  {
            throw MUJIN_EXCEPTION_FORMAT("could not retrieve file data for %s", encoding::ConvertUTF16ToFileSystemEncoding(copydir), MEC_Assert);
        }

        do {
            std::wstring filename = std::wstring(ffd.cFileName);
            if( filename != L"." && filename != L".." ) {
                std::string filename_utf8;
                utf8::utf16to8(filename.begin(), filename.end(), std::back_inserter(filename_utf8));
                std::wstring newcopydir = str(boost::wformat(L"%s\\%s")%copydir%filename);
                char* pescapeddir = curl_easy_escape(_curl, filename_utf8.c_str(), filename_utf8.size());
                std::string newuri = str(boost::format("%s/%s")%uri%pescapeddir);
                curl_free(pescapeddir);

                if( ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
                    _UploadDirectoryToWebDAV_UTF16(newcopydir, newuri);
                }
                else if( ffd.dwFileAttributes == 0 || ffd.dwFileAttributes == FILE_ATTRIBUTE_READONLY || ffd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL || ffd.dwFileAttributes == FILE_ATTRIBUTE_ARCHIVE ) {
                    _UploadFileToWebDAV_UTF16(newcopydir, newuri);
                }
            }
        } while(FindNextFileW(hFind,&ffd) != 0);

        DWORD err = GetLastError();
        FindClose(hFind);
        if( err !=  ERROR_NO_MORE_FILES ) {
            throw MUJIN_EXCEPTION_FORMAT("system error 0x%x when recursing through %s", err%encoding::ConvertUTF16ToFileSystemEncoding(copydir), MEC_HTTPServer);
        }

#else
        boost::filesystem::path bfpcopydir(copydir);
        for(boost::filesystem::directory_iterator itdir(bfpcopydir); itdir != boost::filesystem::directory_iterator(); ++itdir) {
#if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
            std::string dirfilename = encoding::ConvertFileSystemEncodingToUTF8(itdir->path().filename().string());
#else
            std::string dirfilename = encoding::ConvertFileSystemEncodingToUTF8(itdir->path().filename());
#endif
            char* pescapeddir = curl_easy_escape(_curl, dirfilename.c_str(), dirfilename.size());
            std::string newuri = str(boost::format("%s/%s")%uri%pescapeddir);
            curl_free(pescapeddir);
            if( boost::filesystem::is_directory(itdir->status()) ) {
                _UploadDirectoryToWebDAV_UTF8(itdir->path().string(), newuri);
            }
            else if( boost::filesystem::is_regular_file(itdir->status()) ) {
                _UploadFileToWebDAV_UTF8(itdir->path().string(), newuri);
            }
        }
#endif // defined(_WIN32) || defined(_WIN64)
    }

    /// \brief uploads a single file, assumes the directory already exists
    ///
    /// overwrites the file if it already exists
    /// \param filename utf-8 encoded
	void _UploadFileToWebDAV_UTF8(const std::string& filename, const std::string& uri)
    {
        std::string sFilename_FS = encoding::ConvertUTF8ToFileSystemEncoding(filename);
        FileHandler handler(sFilename_FS.c_str());
        if(!handler._fd) {
            throw MUJIN_EXCEPTION_FORMAT("failed to open filename %s for uploading", sFilename_FS, MEC_InvalidArguments);
        }
        _UploadFileToWebDAV(handler._fd, uri);
    }

    void _UploadFileToWebDAV_UTF16(const std::wstring& filename, const std::string& uri)
    {
        std::string filename_fs = encoding::ConvertUTF16ToFileSystemEncoding(filename);
#if defined(_WIN32) || defined(_WIN64)
        FileHandler handler(filename.c_str());
#else
        // linux does not support wide-char fopen        
        FileHandler handler(filename_fs.c_str());
#endif
        if(!handler._fd) {
            throw MUJIN_EXCEPTION_FORMAT("failed to open filename %s for uploading", filename_fs, MEC_InvalidArguments);
        }
        _UploadFileToWebDAV(handler._fd, uri);
    }

    /// \brief uploads a single file, assumes the directory already exists
    ///
    /// overwrites the file if it already exists.
    /// \param fd FILE pointer of binary reading file. does not close the handle
    void _UploadFileToWebDAV(FILE* fd, const std::string& uri)
    {
#if defined(_WIN32) || defined(_WIN64)
        fseek(fd,0,SEEK_END);
        curl_off_t filesize = ftell(fd);
        fseek(fd,0,SEEK_SET);
#else
        // to get the file size
        struct stat file_info;
        if(fstat(fileno(fd), &file_info) != 0) {
            throw MUJIN_EXCEPTION_FORMAT("failed to stat %s for filesize", uri, MEC_InvalidArguments);
        }
        curl_off_t filesize = (curl_off_t)file_info.st_size;
#endif

        // tell it to "upload" to the URL
        CurlUploadSetter uploadsetter(_curl);
        curl_easy_setopt(_curl, CURLOPT_HTTPGET, 0L);
        curl_easy_setopt(_curl, CURLOPT_URL, uri.c_str());
        curl_easy_setopt(_curl, CURLOPT_READDATA, fd);
        curl_easy_setopt(_curl, CURLOPT_INFILESIZE_LARGE, filesize);
        //curl_easy_setopt(_curl, CURLOPT_NOBODY, 1L);
#if defined(_WIN32) || defined(_WIN64)
        curl_easy_setopt(_curl, CURLOPT_READFUNCTION, _ReadUploadCallback);
#endif

        CURLcode res = curl_easy_perform(_curl);
        CHECKCURLCODE(res, "curl_easy_perform failed");
        long http_code = 0;
        res=curl_easy_getinfo (_curl, CURLINFO_RESPONSE_CODE, &http_code);
        // 204 is when it overwrites the file?
        if( http_code != 201 && http_code != 204 ) {
            if( http_code == 400 ) {
                throw MUJIN_EXCEPTION_FORMAT("upload of %s failed with HTTP status %s, perhaps file exists already?", uri%http_code, MEC_HTTPServer);
            }
            else {
                throw MUJIN_EXCEPTION_FORMAT("upload of %s failed with HTTP status %s", uri%http_code, MEC_HTTPServer);
            }
        }
        // now extract transfer info
        //double speed_upload, total_time;
        //curl_easy_getinfo(_curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
        //curl_easy_getinfo(_curl, CURLINFO_TOTAL_TIME, &total_time);
        //printf("http code: %d, Speed: %.3f bytes/sec during %.3f seconds\n", http_code, speed_upload, total_time);
    }

    /// \brief read upload function for win32.
    /// MUST also provide this read callback using CURLOPT_READFUNCTION. Failing to do so will give you a crash since a DLL may not use the variable's memory when passed in to it from an app like this. */
    static size_t _ReadUploadCallback(void *ptr, size_t size, size_t nmemb, void *stream)
    {
        curl_off_t nread;
        // in real-world cases, this would probably get this data differently as this fread() stuff is exactly what the library already would do by default internally
        size_t retcode = fread(ptr, size, nmemb, (FILE*)stream);

        nread = (curl_off_t)retcode;
        //fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T " bytes from file\n", nread);
        return retcode;
    }

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

TaskResourcePtr SceneResource::GetTaskFromName(const std::string& taskname)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("scene/%s/task/?format=json&limit=1&name=%s&fields=pk,tasktype")%GetPrimaryKey()%taskname), pt);
    // task exists
    boost::property_tree::ptree& objects = pt.get_child("objects");
    if( objects.size() == 0 ) {
        throw MUJIN_EXCEPTION_FORMAT("could not find task with name %s", taskname, MEC_InvalidState);
    }

    std::string pk = objects.begin()->second.get<std::string>("pk");
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

bool TaskResource::Execute()
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallPost(str(boost::format("task/%s/")%GetPrimaryKey()), std::string(), pt, 200);
    _jobpk = pt.get<std::string>("jobpk");
    return true;
}

void TaskResource::GetRunTimeStatus(JobStatus& status, int options)
{
    if( _jobpk.size() > 0 ) {
        GETCONTROLLERIMPL();
        boost::property_tree::ptree pt;
        std::string url = str(boost::format("job/%s/?format=json&fields=pk,status,fnname,elapsedtime")%_jobpk);
        if( options & 1 ) {
            url += std::string(",status_text");
        }
        controller->CallGet(url, pt);
        //pt.get("error_message")
        status.pk = pt.get<std::string>("pk");
        status.code = static_cast<JobStatusCode>(boost::lexical_cast<int>(pt.get<std::string>("status")));
        status.type = pt.get<std::string>("fnname");
        status.elapsedtime = pt.get<double>("elapsedtime");
        if( options & 1 ) {
            status.message = pt.get<std::string>("status_text");
        }
    }

    status.code = JSC_Unknown;
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

void TaskResource::GetTaskParameters(ITLPlanningTaskParameters& taskparameters)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("task/%s/?format=json&fields=taskparameters,tasktype")%GetPrimaryKey()), pt);
    std::string tasktype = pt.get<std::string>("tasktype");
    if( tasktype != "itlplanning" ) {
        throw MUJIN_EXCEPTION_FORMAT("task %s is type %s, expected itlplanning", GetPrimaryKey()%tasktype, MEC_InvalidArguments);
    }
    boost::property_tree::ptree& taskparametersjson = pt.get_child("taskparameters");
    taskparameters.SetDefaults();
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, taskparametersjson) {
        if( v.first == "startfromcurrent" ) {
            taskparameters.startfromcurrent = v.second.data() == std::string("True");
        }
        else if( v.first == "returntostart" ) {
            taskparameters.returntostart = v.second.data() == std::string("True");
        }
        else if( v.first == "ignorefigure" ) {
            taskparameters.ignorefigure = v.second.data() == std::string("True");
        }
        else if( v.first == "vrcruns" ) {
            taskparameters.vrcruns = boost::lexical_cast<int>(v.second.data());
        }
        else if( v.first == "unit" ) {
            taskparameters.unit = v.second.data();
        }
        else if( v.first == "optimizationvalue" ) {
            taskparameters.optimizationvalue = boost::lexical_cast<Real>(v.second.data());
        }
        else if( v.first == "program" ) {
            taskparameters.program = v.second.data();
        }
    }
}

void TaskResource::SetTaskParameters(const ITLPlanningTaskParameters& taskparameters)
{
    GETCONTROLLERIMPL();
    std::string startfromcurrent = taskparameters.startfromcurrent ? "True" : "False";
    std::string returntostart = taskparameters.returntostart ? "True" : "False";
    std::string ignorefigure = taskparameters.ignorefigure ? "True" : "False";
    std::string vrcruns = boost::lexical_cast<std::string>(taskparameters.vrcruns);

    // because program will inside string, encode newlines
    std::string program;
    std::vector< std::pair<std::string, std::string> > serachpairs(2);
    serachpairs[0].first = "\n"; serachpairs[0].second = "\\n";
    serachpairs[1].first = "\r\n"; serachpairs[1].second = "\\n";
    SearchAndReplace(program, taskparameters.program, serachpairs);
    std::string taskgoalput = str(boost::format("{\"tasktype\": \"itlplanning\", \"taskparameters\":{\"optimizationvalue\":%f, \"program\":\"%s\", \"unit\":\"%s\", \"returntostart\":\"%s\", \"startfromcurrent\":\"%s\", \"ignorefigure\":\"%s\", \"vrcruns\":%d} }")%taskparameters.optimizationvalue%program%taskparameters.unit%returntostart%startfromcurrent%ignorefigure%vrcruns);
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

void OptimizationResource::SetOptimizationParameters(const RobotPlacementOptimizationParameters& optparams)
{
    GETCONTROLLERIMPL();
    std::string ignorebasecollision = optparams.ignorebasecollision ? "True" : "False";
    std::string optimizationgoalput = str(boost::format("{\"optimizationtype\": \"robotplacement\", \"optimizationparameters\":{\"targetname\":\"%s\", \"frame\":\"%s\", \"ignorebasecollision\":\"%s\", \"unit\":\"%s\", \"maxrange_\":[ %.15f, %.15f, %.15f, %.15f],  \"minrange_\":[ %.15f, %.15f, %.15f, %.15f], \"stepsize_\":[ %.15f, %.15f, %.15f, %.15f], \"topstorecandidates\":%d} }")%optparams.targetname%optparams.frame%ignorebasecollision%optparams.unit%optparams.maxrange[0]%optparams.maxrange[1]%optparams.maxrange[2]%optparams.maxrange[3]%optparams.minrange[0]%optparams.minrange[1]%optparams.minrange[2]%optparams.minrange[3]%optparams.stepsize[0]%optparams.stepsize[1]%optparams.stepsize[2]%optparams.stepsize[3]%optparams.topstorecandidates);
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

void PlanningResultResource::GetEnvironmentState(EnvironmentState& envstate)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("%s/%s/?format=json&fields=envstate")%GetResourceName()%GetPrimaryKey()), pt);
    boost::property_tree::ptree& envstatejson = pt.get_child("envstate");
    envstate.clear();
    BOOST_FOREACH(boost::property_tree::ptree::value_type &objstatejson, envstatejson) {
        InstanceObjectState objstate;
        std::string name = objstatejson.second.get<std::string>("name");
        boost::property_tree::ptree& quatjson = objstatejson.second.get_child("quat_");
        int iquat=0;
        Real dist2 = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, quatjson) {
            BOOST_ASSERT(iquat<4);
            Real f = boost::lexical_cast<Real>(v.second.data());
            dist2 += f * f;
            objstate.transform.quat[iquat++] = f;
        }
        // normalize the quaternion
        if( dist2 > 0 ) {
            Real fnorm =1/std::sqrt(dist2);
            objstate.transform.quat[0] *= fnorm;
            objstate.transform.quat[1] *= fnorm;
            objstate.transform.quat[2] *= fnorm;
            objstate.transform.quat[3] *= fnorm;
        }
        boost::property_tree::ptree& translationjson = objstatejson.second.get_child("translation_");
        int itranslation=0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, translationjson) {
            BOOST_ASSERT(iquat<3);
            objstate.transform.translation[itranslation++] = boost::lexical_cast<Real>(v.second.data());
        }
        envstate[name] = objstate;
    }
}

void PlanningResultResource::GetAllRawProgramData(std::string& outputdata, const std::string& programtype)
{
    GETCONTROLLERIMPL();
    controller->CallGet(str(boost::format("%s/%s/program/?format=json&type=%s")%GetResourceName()%GetPrimaryKey()%programtype), outputdata);
}

void PlanningResultResource::GetRobotRawProgramData(std::string& outputdata, const std::string& robotpk, const std::string& programtype)
{
    GETCONTROLLERIMPL();
    controller->CallGet(str(boost::format("%s/%s/program/%s/?format=json&type=%s")%GetResourceName()%GetPrimaryKey()%robotpk%programtype), outputdata);
}

void PlanningResultResource::GetPrograms(RobotControllerPrograms& programs, const std::string& programtype)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    programs.programs.clear();
    controller->CallGet(str(boost::format("%s/%s/program/?format=json&type=%s")%GetResourceName()%GetPrimaryKey()%programtype), pt);
    BOOST_FOREACH(boost::property_tree::ptree::value_type &robotdatajson, pt) {
        std::string robotpk = robotdatajson.first;
        std::string program = robotdatajson.second.get<std::string>("program");
        std::string currenttype = robotdatajson.second.get<std::string>("type");
        programs.programs[robotpk] = RobotProgramData(program, currenttype);
    }
}

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
