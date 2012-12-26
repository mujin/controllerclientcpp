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

#include <cstdio>
#include <curl/curl.h>


namespace mujinclient {

static int _writer(char *data, size_t size, size_t nmemb, std::string *writerData)
{
    if (writerData == NULL) {
        return 0;
    }
    writerData->append(data, size*nmemb);
    return size * nmemb;
}

#define SKIP_PEER_VERIFICATION // temporary

std::string ListAllEnvironmentsRaw(const std::string& usernamepassword)
{

    std::string buffer;
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://controller.mujin.co.jp/api/v1/scene/?format=json");

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
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0l);
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

        res = curl_easy_setopt(curl, CURLOPT_USERPWD, usernamepassword.c_str());
        if (res != CURLE_OK) {
            throw mujin_exception("failed to set userpw");
        }
        //densowave:Debpawm8
        res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _writer);
        if (res != CURLE_OK) {
            throw mujin_exception("failed to set writer");
        }

        res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        if (res != CURLE_OK) {
            throw mujin_exception("failed to set write data");
        }

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    return buffer;
}



}
