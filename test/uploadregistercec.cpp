// -*- coding: utf-8 -*-
#include "mujintestutils.h"

using namespace mujinclient;

int main(int argc, char ** argv)
{
    try {
        ControllerClientPtr controller = CreateControllerFromCommandLine(argc,argv);

        std::string sourcefilename_utf8 = "../share/mujincontrollerclient/robodia_demo1/robodia_demo1.xml";
        std::string uri_utf8 = "mujin:/testupload/";

        controller->SyncUpload_UTF8(sourcefilename_utf8, uri_utf8, "cecrobodiaxml");

        controller->DeleteDirectoryOnController_UTF8(uri_utf8);
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    ControllerClientDestroy();
}
