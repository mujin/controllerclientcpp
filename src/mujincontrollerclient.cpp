// -*- coding: utf-8 -*-
// Copyright (C) 2012 MUJIN Inc. <rosen.diankov@mujin.co.jp>
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
#include <mujincontrollerclient/mujincontrollerclient.h>

#include <boost/thread/mutex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>

#include <sstream>
#include <curl/curl.h>

#define GETCONTROLLERIMPL() ControllerClientImplPtr controller = boost::dynamic_pointer_cast<ControllerClientImpl>(GetController());

#define MUJIN_EXCEPTION_FORMAT0(s, errorcode) mujinclient::mujin_exception(boost::str(boost::format("[%s:%d] " s)%(__PRETTY_FUNCTION__)%(__LINE__)),errorcode)

/// adds the function name and line number to an exception
#define MUJIN_EXCEPTION_FORMAT(s, args,errorcode) mujinclient::mujin_exception(boost::str(boost::format("[%s:%d] " s)%(__PRETTY_FUNCTION__)%(__LINE__)%args),errorcode)

#define MUJIN_ASSERT_FORMAT(testexpr, s, args, errorcode) { if( !(testexpr) ) { throw mujinclient::mujin_exception(boost::str(boost::format("[%s:%d] (%s) failed " s)%(__PRETTY_FUNCTION__)%(__LINE__)%(# testexpr)%args),errorcode); } }

#define MUJIN_ASSERT_FORMAT0(testexpr, s, errorcode) { if( !(testexpr) ) { throw mujinclient::mujin_exception(boost::str(boost::format("[%s:%d] (%s) failed " s)%(__PRETTY_FUNCTION__)%(__LINE__)%(# testexpr)),errorcode); } }

// note that expr1 and expr2 will be evaluated twice if not equal
#define MUJIN_ASSERT_OP_FORMAT(expr1,op,expr2,s, args, errorcode) { if( !((expr1) op (expr2)) ) { throw mujinclient::mujin_exception(boost::str(boost::format("[%s:%d] %s %s %s, (eval %s %s %s) " s)%(__PRETTY_FUNCTION__)%(__LINE__)%(# expr1)%(# op)%(# expr2)%(expr1)%(# op)%(expr2)%args),errorcode); } }

#define MUJIN_ASSERT_OP_FORMAT0(expr1,op,expr2,s, errorcode) { if( !((expr1) op (expr2)) ) { throw mujinclient::mujin_exception(boost::str(boost::format("[%s:%d] %s %s %s, (eval %s %s %s) " s)%(__PRETTY_FUNCTION__)%(__LINE__)%(# expr1)%(# op)%(# expr2)%(expr1)%(# op)%(expr2)),errorcode); } }

#define MUJIN_ASSERT_OP(expr1,op,expr2) { if( !((expr1) op (expr2)) ) { throw mujinclient::mujin_exception(boost::str(boost::format("[%s:%d] %s %s %s, (eval %s %s %s) ")%(__PRETTY_FUNCTION__)%(__LINE__)%(# expr1)%(# op)%(# expr2)%(expr1)%(# op)%(expr2)),MEC_Assert); } }

namespace mujinclient {

#define SKIP_PEER_VERIFICATION // temporary

class ControllerClientImpl : public ControllerClient
{
public:
    ControllerClientImpl(const std::string& usernamepassword)
    {
        _baseuri = "https://controller.mujin.co.jp/api/v1/";
        //CURLcode code = curl_global_init(CURL_GLOBAL_SSL|CURL_GLOBAL_WIN32);
        _curl = curl_easy_init();
        BOOST_ASSERT(!!_curl);

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

//#ifdef SKIP_HOSTNAME_VERIFICATION
//        /*
//         * If the site you're connecting to uses a different host name that what
//         * they have mentioned in their server certificate's commonName (or
//         * subjectAltName) fields, libcurl will refuse to connect. You can skip
//         * this check, but this will make the connection less secure.
//         */
//        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
//#endif

        res = curl_easy_setopt(_curl, CURLOPT_USERPWD, usernamepassword.c_str());
        if (res != CURLE_OK) {
            throw mujin_exception("failed to set userpw");
        }
        //densowave:Debpawm8
        res = curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, _writer);
        if (res != CURLE_OK) {
            throw mujin_exception("failed to set writer");
        }

        res = curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &_buffer);
        if (res != CURLE_OK) {
            throw mujin_exception("failed to set write data");
        }

    }
    virtual ~ControllerClientImpl() {
        curl_easy_cleanup(_curl);
    }

    virtual void GetSceneFilenames(const std::string& scenetype, std::vector<std::string>& scenefilenames)
    {
    }

    virtual void GetScenePrimaryKeys(std::vector<std::string>& scenekeys)
    {
        _Call("scene/?format=json&limit=0&fields=pk");
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(_buffer, pt);
        boost::property_tree::ptree& objects = pt.get_child("objects");
        scenekeys.resize(objects.size());
        size_t i = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
            scenekeys[i++] = v.second.get<std::string>("pk");
        }
    }

    void _Call(const std::string& relativeuri) {
        boost::mutex::scoped_lock lock(_mutex);
        _uri = _baseuri;
        _uri += relativeuri;
        curl_easy_setopt(_curl, CURLOPT_URL, _uri.c_str());
        _buffer.clear();
        _buffer.str("");
        CURLcode res = curl_easy_perform(_curl);
        if (res != CURLE_OK) {
            throw mujin_exception("curl_easy_perform failed");
        }
    }

    std::stringstream& GetBuffer() {
        return _buffer;
    }

protected:

    static int _writer(char *data, size_t size, size_t nmemb, std::stringstream *writerData)
    {
        if (writerData == NULL) {
            return 0;
        }
        writerData->write(data, size*nmemb);
        return size * nmemb;
    }

    CURL *_curl;
    boost::mutex _mutex;
    std::stringstream _buffer;
    std::string _baseuri, _uri;
};

typedef boost::shared_ptr<ControllerClientImpl> ControllerClientImplPtr;

WebResource::WebResource(ControllerClientPtr controller, const std::string& resourcename, const std::string& pk) : __controller(controller), __resourcename(resourcename), __pk(pk)
{
}

std::string WebResource::Get(const std::string& field)
{
    GETCONTROLLERIMPL();
    controller->_Call(str(boost::format("%s/%s/?format=json&fields=%s")%GetResourceName()%GetPrimaryKey()%field));
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(controller->GetBuffer(), pt);
    std::string fieldvalue = pt.get<std::string>(field);
    return fieldvalue;
}

SceneResource::SceneResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller, "scene", pk)
{
}

TaskResourcePtr SceneResource::GetOrCreateTaskFromName(const std::string& taskname)
{
    GETCONTROLLERIMPL();
    controller->_Call(str(boost::format("scene/%s/task/?format=json&limit=1&name=%s&fields=pk")%GetPrimaryKey()%taskname));
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(controller->GetBuffer(), pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    if( objects.size() == 0 ) {
        throw MUJIN_EXCEPTION_FORMAT("failed to get task %s", taskname, MEC_InvalidArguments);
    }
    std::string pk = objects.begin()->second.get<std::string>("pk");
    TaskResourcePtr task(new TaskResource(GetController(), pk));
    return task;
}

TaskResource::TaskResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller,"task",pk)
{
}

void TaskResource::Execute()
{
    throw mujin_exception("not implemented yet");
}

PlanningResultResourcePtr TaskResource::GetTaskResult()
{
    GETCONTROLLERIMPL();
    controller->_Call(str(boost::format("task/%s/result/?format=json&limit=1&optimization=None&fields=pk")%GetPrimaryKey()));
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(controller->GetBuffer(), pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    if( objects.size() == 0 ) {
        return PlanningResultResourcePtr();
    }
    std::string pk = objects.begin()->second.get<std::string>("pk");
    PlanningResultResourcePtr result(new PlanningResultResource(GetController(), pk));
    return result;
}

PlanningResultResource::PlanningResultResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller,"planningresult",pk)
{
}

ControllerClientPtr CreateControllerClient(const std::string& usernamepassword)
{
    return ControllerClientPtr(new ControllerClientImpl(usernamepassword));
}

void ControllerClientDestroy()
{
}

}
