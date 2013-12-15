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
#include <boost/thread.hpp> // for sleep

namespace mujinclient {

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

ObjectResource::ObjectResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller, "object", pk)
{
}

ObjectResource::ObjectResource(ControllerClientPtr controller, const std::string& resource, const std::string& pk) : WebResource(controller, resource, pk)
{
}

ObjectResource::LinkResource::LinkResource(ControllerClientPtr controller, const std::string& objectpk, const std::string& pk) : WebResource(controller, str(boost::format("object/%s/link")%objectpk), pk)
{
}

void ObjectResource::GetLinks(std::vector<ObjectResource::LinkResourcePtr>& links)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("object/%s/link/?format=json&limit=0&fields=links")%GetPrimaryKey()), pt);
    boost::property_tree::ptree& objects = pt.get_child("links");
    links.resize(objects.size());
    size_t i = 0;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
        LinkResourcePtr link(new LinkResource(controller, GetPrimaryKey(), v.second.get<std::string>("pk")));


        link->name = v.second.get<std::string>("name");
        link->pk = v.second.get<std::string>("pk");

        boost::property_tree::ptree& jsonattachments = v.second.get_child("attachmentpks");
        link->attachmentpks.resize(jsonattachments.size());
        size_t iattch = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &att, jsonattachments) {
            link->attachmentpks[iattch++] = att.second.data();
        }

        //TODO transforms
        links[i++] = link;
    }
}

RobotResource::RobotResource(ControllerClientPtr controller, const std::string& pk) : ObjectResource(controller, "robot", pk)
{
}

RobotResource::ToolResource::ToolResource(ControllerClientPtr controller, const std::string& robotobjectpk, const std::string& pk) : WebResource(controller, str(boost::format("robot/%s/tool")%robotobjectpk), pk)
{
}

void RobotResource::GetTools(std::vector<RobotResource::ToolResourcePtr>& tools)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("robot/%s/tool/?format=json&limit=0&fields=tools")%GetPrimaryKey()), pt);
    boost::property_tree::ptree& objects = pt.get_child("tools");
    tools.resize(objects.size());
    size_t i = 0;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
        ToolResourcePtr tool(new ToolResource(controller, GetPrimaryKey(), v.second.get<std::string>("pk")));


        tool->name = v.second.get<std::string>("name");
        tool->pk = v.second.get<std::string>("pk");
        tool->frame_origin = v.second.get<std::string>("frame_origin");
        tool->frame_tip = v.second.get<std::string>("frame_tip");

        boost::property_tree::ptree& jsondirection = v.second.get_child("direction");
        size_t idir = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vdir, jsondirection) {
            tool->direction[idir++] = boost::lexical_cast<Real>(vdir.second.data());
        }

        size_t iquaternion = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vquaternion, v.second.get_child("quaternion")) {
            BOOST_ASSERT( iquaternion < 4 );
            tool->quaternion[iquaternion++] = boost::lexical_cast<Real>(vquaternion.second.data());
        }
        size_t itranslate = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vtranslate, v.second.get_child("translate")) {
            BOOST_ASSERT( itranslate < 3 );
            tool->translate[itranslate++] = boost::lexical_cast<Real>(vtranslate.second.data());
        }

        tools[i++] = tool;
    }
}

SceneResource::InstObject::InstObject(ControllerClientPtr controller, const std::string& scenepk, const std::string& pk) : WebResource(controller, str(boost::format("scene/%s/instobject")%scenepk), pk)
{
}

void SceneResource::InstObject::SetTransform(const Transform& t)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    std::string data = str(boost::format("{\"quaternion\":[%.15f, %.15f, %.15f, %.15f], \"translate\":[%.15f, %.15f, %.15f]}")%t.quaternion[0]%t.quaternion[1]%t.quaternion[2]%t.quaternion[3]%t.translate[0]%t.translate[1]%t.translate[2]);
    controller->CallPut(str(boost::format("%s/%s/?format=json")%GetResourceName()%GetPrimaryKey()), data, pt);
}

void SceneResource::InstObject::SetDOFValues()
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    std::stringstream ss;
    ss << "{\"dofvalues\":";
    ss << "[";
    if( this->dofvalues.size() > 0 ) {
        for (unsigned int i = 0; i < this->dofvalues.size(); i++) {
            ss << this->dofvalues[i];
            if( i != this->dofvalues.size()-1) {
                ss << ", ";
            }
        }
    }
    ss << "]}";
    controller->CallPut(str(boost::format("%s/%s/?format=json")%GetResourceName()%GetPrimaryKey()), ss.str(), pt);
}


void SceneResource::InstObject::GrabObject(InstObjectPtr grabbedobject, std::string& grabbedobjectlinkpk, std::string& grabbinglinkpk)
{
    SceneResource::InstObject::Grab grab;
    grab.instobjectpk = grabbedobject->pk;
    grab.grabbed_linkpk = grabbedobjectlinkpk;
    grab.grabbing_linkpk = grabbinglinkpk;
    for (size_t igrab = 0; igrab < this->grabs.size(); igrab++) {
        if (this->grabs[igrab] == grab) {
            std::cerr << grabbedobject->name << "is already grabbed" << std::endl;
            return;
        }
    }
    std::stringstream ss;
    ss << "{\"grabs\":";
    ss << "[";
    if( this->grabs.size() > 0 ) {
        for (unsigned int i = 0; i < this->grabs.size(); i++) {
            ss << this->grabs[i].Serialize() << ", ";
        }
    }
    ss << grab.Serialize();
    ss << "]}";
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("%s/%s/?format=json")%GetResourceName()%GetPrimaryKey()), ss.str(), pt);
}

void SceneResource::InstObject::ReleaseObject(InstObjectPtr grabbedobject, std::string& grabbedobjectlinkpk, std::string& grabbinglinkpk)
{
    SceneResource::InstObject::Grab grab;
    grab.instobjectpk = grabbedobject->pk;
    grab.grabbed_linkpk = grabbedobjectlinkpk;
    grab.grabbing_linkpk = grabbinglinkpk;
    for (size_t igrab = 0; igrab < this->grabs.size(); igrab++) {
        if (this->grabs[igrab] == grab) {
            this->grabs.erase(std::remove(this->grabs.begin(), this->grabs.end(), this->grabs[igrab]), this->grabs.end());
            std::stringstream ss;
            ss << "{\"grabs\":";
            ss << "[";
            if( this->grabs.size() > 0 ) {
                for (unsigned int i = 0; i < this->grabs.size(); i++) {
                    ss << this->grabs[i].Serialize() << ", ";
                }
                if( igrab != this->grabs.size()-1) {
                    ss << ", ";
                }
            }
            ss << "]}";
            GETCONTROLLERIMPL();
            boost::property_tree::ptree pt;
            controller->CallPut(str(boost::format("%s/%s/?format=json")%GetResourceName()%GetPrimaryKey()), ss.str(), pt);
        }
    }
    std::cerr << grabbedobject->name << "is not grabbed" << std::endl;

}

void SceneResource::InstObject::Print()
{
    std::cout << "dofvalues: ";
    for (int i = 0; i < dofvalues.size(); i++) {
        std::cout << dofvalues[i] << ", ";
    }
    std::cout << std::endl;
    std::cout << "name: " << name << std::endl;
    std::cout << "pk: " <<  pk << std::endl;
    std::cout << "object_pk: " << object_pk << std::endl;
    std::cout << "reference_uri: " << reference_uri << std::endl;

    std::cout << "quaternion: ";
    for (int i = 0; i < 4; i++) {
        std::cout << quaternion[i] << ", ";
    }
    std::cout << std::endl;
    std::cout << "translate: ";
    for (int i = 0; i < 3; i++) {
        std::cout << translate[i] << ", ";
    }
    std::cout << std::endl;

    std::cout << "links: " << std::endl;
    for (int i = 0; i < links.size(); i++) {
        std::cout << "\tname: " << links[i].name << std::endl;
        std::cout << "\tquaternion: ";
        for (int i = 0; i < 4; i++) {
            std::cout << links[i].quaternion[i] << ", ";
        }
        std::cout << std::endl;
        std::cout << "\ttranslate: ";
        for (int i = 0; i < 3; i++) {
            std::cout << links[i].translate[i] << ", ";
        }
        std::cout << std::endl;
    }

    std::cout << "tools: " << std::endl;
    for (int i = 0; i < tools.size(); i++) {
        std::cout << "\tname: " << tools[i].name << std::endl;

        std::cout << "\tquaternion: ";
        for (int i = 0; i < 4; i++) {
            std::cout << tools[i].quaternion[i] << ", ";
        }
        std::cout << std::endl;

        std::cout << "\ttranslate: ";
        for (int i = 0; i < 3; i++) {
            std::cout << tools[i].translate[i] << ", ";
        }
        std::cout << std::endl;

        std::cout << "\tdirection: ";
        for (int i = 0; i < 3; i++) {
            std::cout << tools[i].direction[i] << ", ";
        }
        std::cout << std::endl;
    }
}

SceneResource::SceneResource(ControllerClientPtr controller, const std::string& pk) : WebResource(controller, "scene", pk)
{
    // get something from the scene?
    //this->Get("");
}

TaskResourcePtr SceneResource::GetOrCreateTaskFromName_UTF8(const std::string& taskname, const std::string& tasktype)
{
    GETCONTROLLERIMPL();
    boost::shared_ptr<char> pescapedtaskname = controller->GetURLEscapedString(taskname);
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("scene/%s/task/?format=json&limit=1&name=%s&fields=pk,tasktype")%GetPrimaryKey()%pescapedtaskname), pt);
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

void SceneResource::SetInstObjectsState(const std::vector<SceneResource::InstObjectPtr>& instobjects, const std::vector<InstanceObjectState>& states)
{
    GETCONTROLLERIMPL();
    MUJIN_ASSERT_OP_FORMAT(instobjects.size(), ==, states.size(), "the size of instobjects (%d) and the one of states (%d) must be the same",instobjects.size()%states.size(),MEC_InvalidArguments);
    boost::property_tree::ptree pt;
    boost::format transformtemplate("{\"pk\":\"%s\",\"quaternion\":[%.15f, %.15f, %.15f, %.15f], \"translate\":[%.15f, %.15f, %.15f] %s}");
    std::string datastring, sdofvalues;
    for(size_t i = 0; i < instobjects.size(); ++i) {
        const Transform& transform = states[i].transform;
        if( states[i].dofvalues.size() > 0 ) {
            sdofvalues = str(boost::format(", \"dofvalues\":[%.15f")%states[i].dofvalues.at(0));
            for(size_t j = 1; j < states[i].dofvalues.size(); ++j) {
                sdofvalues += str(boost::format(", %.15f")%states[i].dofvalues.at(j));
            }
            sdofvalues += "]";
        }
        else {
            sdofvalues = "";
        }
        datastring += str(transformtemplate%instobjects[i]->pk%transform.quaternion[0]%transform.quaternion[1]%transform.quaternion[2]%transform.quaternion[3]%transform.translate[0]%transform.translate[1]%transform.translate[2]%sdofvalues);
        if ( i != instobjects.size()-1) {
            datastring += ",";
        }
    }
    std::string data = str(boost::format("{\"objects\": [%s]}")%datastring);
    controller->CallPut(str(boost::format("%s/%s/instobject/?format=json")%GetResourceName()%GetPrimaryKey()), data, pt);
}

TaskResourcePtr SceneResource::GetTaskFromName_UTF8(const std::string& taskname)
{
    GETCONTROLLERIMPL();
    boost::shared_ptr<char> pescapedtaskname = controller->GetURLEscapedString(taskname);
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("scene/%s/task/?format=json&limit=1&name=%s&fields=pk,tasktype")%GetPrimaryKey()%pescapedtaskname), pt);
    // task exists
    boost::property_tree::ptree& objects = pt.get_child("objects");
    if( objects.size() == 0 ) {
        throw MUJIN_EXCEPTION_FORMAT("could not find task with name %s", taskname, MEC_InvalidState);
    }

    std::string pk = objects.begin()->second.get<std::string>("pk");
    TaskResourcePtr task(new TaskResource(GetController(), pk));
    return task;
}

TaskResourcePtr SceneResource::GetOrCreateTaskFromName_UTF16(const std::wstring& taskname, const std::string& tasktype)
{
    std::string taskname_utf8;
    utf8::utf16to8(taskname.begin(), taskname.end(), std::back_inserter(taskname_utf8));
    return GetOrCreateTaskFromName_UTF8(taskname_utf8, tasktype);
}

TaskResourcePtr SceneResource::GetTaskFromName_UTF16(const std::wstring& taskname)
{
    std::string taskname_utf8;
    utf8::utf16to8(taskname.begin(), taskname.end(), std::back_inserter(taskname_utf8));
    return GetTaskFromName_UTF8(taskname_utf8);
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

        instobject->name = v.second.get<std::string>("name");
        instobject->pk = v.second.get<std::string>("pk");
        instobject->object_pk = v.second.get<std::string>("object_pk");
        instobject->reference_uri = v.second.get<std::string>("reference_uri");

        boost::property_tree::ptree& jsondofvalues = v.second.get_child("dofvalues");
        instobject->dofvalues.resize(jsondofvalues.size());
        size_t idof = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vdof, jsondofvalues) {
            instobject->dofvalues[idof++] = boost::lexical_cast<Real>(vdof.second.data());
        }

        size_t iquaternion = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vquaternion, v.second.get_child("quaternion")) {
            BOOST_ASSERT( iquaternion < 4 );
            instobject->quaternion[iquaternion++] = boost::lexical_cast<Real>(vquaternion.second.data());
        }
        size_t itranslate = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vtranslate, v.second.get_child("translate")) {
            BOOST_ASSERT( itranslate < 3 );
            instobject->translate[itranslate++] = boost::lexical_cast<Real>(vtranslate.second.data());
        }

        boost::property_tree::ptree& jsonlinks = v.second.get_child("links");
        instobject->links.resize(jsonlinks.size());
        size_t ilink = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vlink, jsonlinks) {
            instobject->links[ilink].name = vlink.second.get<std::string>("name");

            boost::property_tree::ptree& quatjson = vlink.second.get_child("quaternion");
            int iquat=0;
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, quatjson) {
                BOOST_ASSERT(iquat<4);
                instobject->links[ilink].quaternion[iquat] = boost::lexical_cast<Real>(v.second.data());
            }

            boost::property_tree::ptree& transjson = vlink.second.get_child("translate");
            int itrans=0;
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, transjson) {
                BOOST_ASSERT(itrans<3);
                instobject->links[ilink].translate[itrans] = boost::lexical_cast<Real>(v.second.data());
            }
            ilink++;
        }

        boost::property_tree::ptree& jsontools = v.second.get_child("tools");
        instobject->tools.resize(jsontools.size());
        size_t itool = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vtool, jsontools) {
            instobject->tools[itool].name = vtool.second.get<std::string>("name");

            boost::property_tree::ptree& quatjson = vtool.second.get_child("quaternion");
            int iquat=0;
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, quatjson) {
                BOOST_ASSERT(iquat<4);
                instobject->tools[itool].quaternion[iquat] = boost::lexical_cast<Real>(v.second.data());
            }

            boost::property_tree::ptree& transjson = vtool.second.get_child("translate");
            int itrans=0;
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, transjson) {
                BOOST_ASSERT(itrans<3);
                instobject->tools[itool].translate[itrans] = boost::lexical_cast<Real>(v.second.data());
            }

            boost::property_tree::ptree& directionjson = vtool.second.get_child("direction");
            int idir=0;
            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, directionjson) {
                BOOST_ASSERT(idir<3);
                instobject->tools[itool].direction[idir] = boost::lexical_cast<Real>(v.second.data());
            }
            itool++;
        }

        boost::property_tree::ptree& jsongrabs = v.second.get_child("grabs");
        instobject->grabs.resize(jsongrabs.size());
        size_t igrab = 0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &vgrab, jsongrabs) {
            instobject->grabs[igrab].instobjectpk = vgrab.second.get<std::string>("instobjectpk");
            instobject->grabs[igrab].grabbed_linkpk = vgrab.second.get<std::string>("grabbed_linkpk");
            instobject->grabs[igrab].grabbing_linkpk = vgrab.second.get<std::string>("grabbing_linkpk");
            igrab++;
        }

        instobjects[i++] = instobject;
    }
}

SceneResource::InstObjectPtr SceneResource::CreateInstObject(const std::string& name, const std::string& reference_uri, Real quaternion[4], Real translation[3])
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallPost(str(boost::format("scene/%s/instobject/?format=json&fields=pk")%GetPrimaryKey()), str(boost::format("{\"name\":\"%s\", \"reference_uri\":\"%s\",\"quaternion\":[%.15f,%.15f,%.15f,%.15f], \"translate\":[%.15f,%.15f,%.15f] }")%name%reference_uri%quaternion[0]%quaternion[1]%quaternion[2]%quaternion[3]%translation[0]%translation[1]%translation[2]), pt);
    std::string inst_pk = pt.get<std::string>("pk");
    SceneResource::InstObjectPtr instobject(new SceneResource::InstObject(GetController(), GetPrimaryKey(),  inst_pk));
    return instobject;
}

SceneResourcePtr SceneResource::Copy(const std::string& name)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallPost("scene/?format=json", str(boost::format("{\"name\":\"%s\", \"reference_pk\":\"%s\", \"overwrite\": \"1\"}")%name%GetPrimaryKey()), pt);
    std::string pk = pt.get<std::string>("pk");
    SceneResourcePtr scene(new SceneResource(GetController(), pk));
    return scene;
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

void TaskResource::Cancel()
{
    // have to look through all jobs for the task
    BOOST_ASSERT(0);
}

void TaskResource::GetRunTimeStatus(JobStatus& status, int options)
{
    status.code = JSC_Unknown;
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
        status.elapsedtime = pt.get<Real>("elapsedtime");
        if( options & 1 ) {
            status.message = pt.get<std::string>("status_text");
        }
    }
}

OptimizationResourcePtr TaskResource::GetOrCreateOptimizationFromName_UTF8(const std::string& optimizationname, const std::string& optimizationtype)
{
    GETCONTROLLERIMPL();
    boost::shared_ptr<char> pescapedoptimizationname = controller->GetURLEscapedString(optimizationname);
    boost::property_tree::ptree pt;
    controller->CallGet(str(boost::format("task/%s/optimization/?format=json&limit=1&name=%s&fields=pk,optimizationtype")%GetPrimaryKey()%pescapedoptimizationname), pt);
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

OptimizationResourcePtr TaskResource::GetOrCreateOptimizationFromName_UTF16(const std::wstring& optimizationname, const std::string& optimizationtype)
{
    std::string optimizationname_utf8;
    utf8::utf16to8(optimizationname.begin(), optimizationname.end(), std::back_inserter(optimizationname_utf8));
    return GetOrCreateOptimizationFromName_UTF8(optimizationname_utf8, optimizationtype);
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

void OptimizationResource::Execute(bool bClearOldResults)
{
    GETCONTROLLERIMPL();
    boost::property_tree::ptree pt;
    controller->CallPost(str(boost::format("optimization/%s/")%GetPrimaryKey()), str(boost::format("{\"clear\":%d}")%bClearOldResults), pt, 200);
    _jobpk = pt.get<std::string>("jobpk");
}

void OptimizationResource::Cancel()
{
    BOOST_ASSERT(0);
}

void OptimizationResource::GetRunTimeStatus(JobStatus& status, int options)
{
    status.code = JSC_Unknown;
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
        status.elapsedtime = pt.get<Real>("elapsedtime");
        if( options & 1 ) {
            status.message = pt.get<std::string>("status_text");
        }
    }
}

void OptimizationResource::SetOptimizationParameters(const RobotPlacementOptimizationParameters& optparams)
{
    GETCONTROLLERIMPL();
    std::string ignorebasecollision = optparams.ignorebasecollision ? "True" : "False";
    std::string optimizationgoalput = str(boost::format("{\"optimizationtype\":\"robotplacement\", \"optimizationparameters\":{\"targetname\":\"%s\", \"frame\":\"%s\", \"ignorebasecollision\":\"%s\", \"unit\":\"%s\", \"maxrange_\":[ %.15f, %.15f, %.15f, %.15f],  \"minrange_\":[ %.15f, %.15f, %.15f, %.15f], \"stepsize_\":[ %.15f, %.15f, %.15f, %.15f], \"topstorecandidates\":%d} }")%optparams.targetname%optparams.framename%ignorebasecollision%optparams.unit%optparams.maxrange[0]%optparams.maxrange[1]%optparams.maxrange[2]%optparams.maxrange[3]%optparams.minrange[0]%optparams.minrange[1]%optparams.minrange[2]%optparams.minrange[3]%optparams.stepsize[0]%optparams.stepsize[1]%optparams.stepsize[2]%optparams.stepsize[3]%optparams.topstorecandidates);
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("optimization/%s/?format=json&fields=")%GetPrimaryKey()), optimizationgoalput, pt);
}

void OptimizationResource::SetOptimizationParameters(const PlacementsOptimizationParameters& optparams)
{
    GETCONTROLLERIMPL();
    std::stringstream optimizationgoalput;
    optimizationgoalput << str(boost::format("{\"optimizationtype\":\"placements\", \"optimizationparameters\":{ \"unit\":\"%s\", \"topstorecandidates\":%d")%optparams.unit%optparams.topstorecandidates);
    for(size_t itarget = 0; itarget < optparams.targetnames.size(); ++itarget) {
        std::string ignorebasecollision = optparams.ignorebasecollisions[itarget] ? "True" : "False";
        std::string suffix;
        if( itarget > 0 ) {
            suffix = boost::lexical_cast<std::string>(itarget+1);
        }
        optimizationgoalput << str(boost::format(", \"targetname%s\":\"%s\", \"frame%s\":\"%s\", \"ignorebasecollision%s\":\"%s\", , \"maxrange%s_\":[ %.15f, %.15f, %.15f, %.15f],  \"minrange%s_\":[ %.15f, %.15f, %.15f, %.15f], \"stepsize%s_\":[ %.15f, %.15f, %.15f, %.15f]")%suffix%optparams.targetnames[itarget]%suffix%optparams.framenames[itarget]%suffix%ignorebasecollision%suffix%optparams.maxranges[itarget][0]%optparams.maxranges[itarget][1]%optparams.maxranges[itarget][2]%optparams.maxranges[itarget][3]%suffix%optparams.minranges[itarget][0]%optparams.minranges[itarget][1]%optparams.minranges[itarget][2]%optparams.minranges[itarget][3]%suffix%optparams.stepsizes[itarget][0]%optparams.stepsizes[itarget][1]%optparams.stepsizes[itarget][2]%optparams.stepsizes[itarget][3]);
    }
    optimizationgoalput << "} }";
    boost::property_tree::ptree pt;
    controller->CallPut(str(boost::format("optimization/%s/?format=json&fields=")%GetPrimaryKey()), optimizationgoalput.str(), pt);
}

void OptimizationResource::GetResults(std::vector<PlanningResultResourcePtr>& results, int startoffset, int num)
{
    GETCONTROLLERIMPL();
    std::string querystring = str(boost::format("optimization/%s/result/?format=json&fields=pk&order_by=task_time&offset=%d&limit=%d")%GetPrimaryKey()%startoffset%num);
    boost::property_tree::ptree pt;
    controller->CallGet(querystring, pt);
    boost::property_tree::ptree& objects = pt.get_child("objects");
    results.resize(objects.size());
    size_t i = 0;
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, objects) {
        results[i++].reset(new PlanningResultResource(controller, v.second.get<std::string>("pk")));
    }
}

PlanningResultResource::PlanningResultResource(ControllerClientPtr controller, const std::string& resulttype, const std::string& pk) : WebResource(controller,resulttype,pk)
{
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
            objstate.transform.quaternion[iquat++] = f;
        }
        // normalize the quaternion
        if( dist2 > 0 ) {
            Real fnorm =1/std::sqrt(dist2);
            objstate.transform.quaternion[0] *= fnorm;
            objstate.transform.quaternion[1] *= fnorm;
            objstate.transform.quaternion[2] *= fnorm;
            objstate.transform.quaternion[3] *= fnorm;
        }
        boost::property_tree::ptree& translationjson = objstatejson.second.get_child("translation_");
        int itranslation=0;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, translationjson) {
            objstate.transform.translate[itranslation++] = boost::lexical_cast<Real>(v.second.data());
        }
        envstate[name] = objstate;
    }
}

void PlanningResultResource::GetAllRawProgramData(std::string& outputdata, const std::string& programtype)
{
    GETCONTROLLERIMPL();
    controller->CallGet(str(boost::format("%s/%s/program/?type=%s")%GetResourceName()%GetPrimaryKey()%programtype), outputdata);
}

void PlanningResultResource::GetRobotRawProgramData(std::string& outputdata, const std::string& robotpk, const std::string& programtype)
{
    GETCONTROLLERIMPL();
    controller->CallGet(str(boost::format("%s/%s/program/%s/?type=%s")%GetResourceName()%GetPrimaryKey()%robotpk%programtype), outputdata);
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
