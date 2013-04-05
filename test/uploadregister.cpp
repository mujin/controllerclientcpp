// -*- coding: utf-8 -*-
#include <mujincontrollerclient/mujincontrollerclient.h>

#include <iostream>

using namespace mujinclient;

int main(int argc, char ** argv)
{
    try {
        ControllerClientPtr controller;
        if( argc >= 5 ) {
            controller = CreateControllerClient(argv[1], argv[2], argv[3], argv[4]);
        }
        if( argc == 4 ) {
            controller = CreateControllerClient(argv[1], argv[2], argv[3]);
        }
        else if( argc == 3 ) {
            controller = CreateControllerClient(argv[1], argv[2]);
        }
        else {
            controller = CreateControllerClient(argv[1]);
        }
        std::cout << "connected to controller v" << controller->GetVersion() << std::endl;

        std::string sourcefilename = "../share/mujincontrollerclient/densowave_wincaps_data/threegoaltouch/threegoaltouch.WPJ";
        //std::string sourcefilename = "F:\\dev\\densowave\\wincaps\\rc8test\\test0\\test0.WPJ";
        std::string sourcefilename_utf8 = "F:\\dev\\densowave\\wincaps\\レイアウト評価\\レイアウト評価.WPJ";
        std::wstring sourcefilename_utf16 = L"F:\\dev\\densowave\\wincaps\\\u30ec\u30a4\u30a2\u30a6\u30c8\u8a55\u4fa1\\\u30ec\u30a4\u30a2\u30a6\u30c8\u8a55\u4fa1.WPJ";
        //controller->SyncUpload_UTF8(sourcefilename, "mujin:/testupload/", "wincaps");
        //controller->SyncUpload_UTF8(sourcefilename_utf8, "mujin:/", "wincaps");
        controller->SyncUpload_UTF16(sourcefilename_utf16, L"mujin:/", "wincaps");
        //SceneResourcePtr scene = controller->RegisterScene("mujin:/densowave_wincaps_data/threegoaltouch/threegoaltouch.WPJ", "wincaps");
    }
    catch(const MujinException& ex) {
        std::cout << "exception thrown: " << ex.message() << std::endl;
    }
    ControllerClientDestroy();
}
