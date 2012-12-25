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

void testfn()
{

    std::string buffer;
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://controller.mujin.co.jp/");

//#ifdef SKIP_PEER_VERIFICATION
//        /*
//         * If you want to connect to a site who isn't using a certificate that is
//         * signed by one of the certs in the CA bundle you have, you can skip the
//         * verification of the server's certificate. This makes the connection
//         * A LOT LESS SECURE.
//         *
//         * If you have a CA cert for the server stored someplace else than in the
//         * default bundle, then the CURLOPT_CAPATH option might come handy for
//         * you.
//         */
//        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
//#endif
//
//#ifdef SKIP_HOSTNAME_VERIFICATION
//        /*
//         * If the site you're connecting to uses a different host name that what
//         * they have mentioned in their server certificate's commonName (or
//         * subjectAltName) fields, libcurl will refuse to connect. You can skip
//         * this check, but this will make the connection less secure.
//         */
//        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
//#endif

        res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _writer);
        if (res != CURLE_OK) {
            //fprintf(stderr, "Failed to set writer [%s]\n", errorBuffer);
            return; // false;
        }

        res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        if (res != CURLE_OK) {
            //fprintf(stderr, "Failed to set write data [%s]\n", errorBuffer);
            return; // false;
        }

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        printf("buffer: %s\n", buffer.c_str());
        /* always cleanup */
        curl_easy_cleanup(curl);
    }
}



}
