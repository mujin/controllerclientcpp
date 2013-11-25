// -*- coding: utf-8 -*-
#include "mujintestutils.h"

using namespace mujinclient;

int main(int argc, char ** argv)
{
    try {
        ControllerClientPtr controller = CreateControllerFromCommandLine(argc,argv);

        std::string sourcefilename_utf8 = "../share/mujincontrollerclient/robodia_demo1/robodia_demo1.xml";
        std::string uridir_utf8 = "mujin:/testupload/";
        std::string uri_utf8 = "mujin:/testupload/robodia_demo1.xml";

        controller->SyncUpload_UTF8(sourcefilename_utf8, uridir_utf8, "cecrobodiaxml");

        controller->RegisterScene_UTF8(uri_utf8, "cecrobodiaxml");

        // extract some information

        // delete
        //controller->DeleteDirectoryOnController_UTF8(uridir_utf8);

        // test if the delete works

    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    ControllerClientDestroy();
}
