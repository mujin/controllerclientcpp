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
#include <boost/property_tree/xml_parser.hpp>
#include <boost/lexical_cast.hpp>
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
        throw MUJIN_EXCEPTION_FORMAT("failed to get task %s from scene pk %s", taskname%GetPrimaryKey(), MEC_InvalidArguments);
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

JobStatus TaskResource::GetRunTimeStatus() {
    throw mujin_exception("not implemented yet");
}

OptimizationResourcePtr TaskResource::GetOrCreateOptimizationFromName(const std::string& optimizationname)
{
    GETCONTROLLERIMPL();
    controller->_Call(str(boost::format("task/%s/optimization/?format=json&limit=1&name=%s&fields=pk")%GetPrimaryKey()%optimizationname));
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(controller->GetBuffer(), pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    if( objects.size() == 0 ) {
        throw MUJIN_EXCEPTION_FORMAT("failed to get optimization %s from task pk", optimizationname%GetPrimaryKey(), MEC_InvalidArguments);
    }
    std::string pk = objects.begin()->second.get<std::string>("pk");
    OptimizationResourcePtr optimization(new OptimizationResource(GetController(), pk));
    return optimization;
}

PlanningResultResourcePtr TaskResource::GetResult()
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

OptimizationResource::OptimizationResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller,"optimization",pk)
{
}

void OptimizationResource::Execute()
{
    throw mujin_exception("not implemented yet");
}

JobStatus OptimizationResource::GetRunTimeStatus() {
    throw mujin_exception("not implemented yet");
}

void OptimizationResource::GetResults(int fastestnum, std::vector<PlanningResultResourcePtr>& results)
{
    GETCONTROLLERIMPL();
    controller->_Call(str(boost::format("optimization/%s/result/?format=json&limit=%d&fields=pk&order_by=task_time")%GetPrimaryKey()%fastestnum));
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(controller->GetBuffer(), pt);
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
    controller->_Call(str(boost::format("%s/%s/?format=json&fields=transformxml")%GetResourceName()%GetPrimaryKey()));
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(controller->GetBuffer(), pt);
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
ControllerClientPtr CreateControllerClient(const std::string& usernamepassword)
{
    return ControllerClientPtr(new ControllerClientImpl(usernamepassword));
}

void ControllerClientDestroy()
{
}

void ComputeMatrixFromTransform(Real matrix[12], const Transform &transform)
{
    throw mujin_exception("not implemented yet");
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
    throw mujin_exception("not implemented yet");
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
    throw mujin_exception("not implemented yet");
    //zxyFromMatrix(matrixFromTransform())
}

}
