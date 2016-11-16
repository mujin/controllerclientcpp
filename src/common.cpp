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

namespace mujinclient {

bool PairStringLengthCompare(const std::pair<std::string, std::string>&p0, const std::pair<std::string, std::string>&p1)
{
    return p0.first.size() > p1.first.size();
}

std::string& SearchAndReplace(std::string& out, const std::string& in, const std::vector< std::pair<std::string, std::string> >&_pairs)
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

const std::map<int, std::string>& GetCodePageMap()
{
    static std::map<int, std::string> s_mapCodePageToCharset = InitializeCodePageMap();
    return s_mapCodePageToCharset;
}


#endif

} // namespace encoding

void ConvertTimestampToFloat(const std::string& in,
                             std::stringstream& outss)
{
    const std::size_t len = in.size();
    std::size_t processed = 0;
    while (processed != std::string::npos && processed < len) {
        const std::size_t deltabegin = in.substr(processed, len).find("\"timestamp\":");
        if (deltabegin == std::string::npos) {
            outss << in.substr(processed, len);
            return;
        }
        const std::size_t timestampbegin = processed + deltabegin;
        const std::size_t comma = in.substr(timestampbegin, len).find(",");
        const std::size_t closingCurly = in.substr(timestampbegin, len).find("}");
        if (comma == std::string::npos && closingCurly == std::string::npos)
        {
            throw mujinclient::MujinException(boost::str(boost::format("error while converting timestamp value format for %s")%in), mujinclient::MEC_Failed);
        }
        const std::size_t timestampend = timestampbegin + (comma < closingCurly ? comma : closingCurly);
        if (timestampend == std::string::npos) {
            throw mujinclient::MujinException(boost::str(boost::format("error while converting timestamp value format for %s")%in), mujinclient::MEC_Failed);
        }
        const std::size_t period = in.substr(timestampbegin, len).find(".");

        if (period == std::string::npos || timestampbegin + period > timestampend) {
            // no comma, assume this is in integet format. so add period to make it a float format.
            outss << in.substr(processed, timestampend - processed) << ".";
        }
        else {
            // already floating format. keep it that way
            outss << in.substr(processed, timestampend - processed);
        }
        processed = timestampend;
    }
}


void ParsePropertyTreeWin(const std::string& originalStr,
		                   boost::property_tree::ptree& pt)
{

    // sometimes buffer can containe \n, \\ and so on, which windows boost property_tree does not like
    std::string newbuffer1, newbuffer2;
    {
        // need to first deal with this case alone. since dealing with "\\\"" and "\\" simultaneously does not give us good result.
        // for example, input string "\\\"", it can be matched by both "\\\"" and "\\" and result is not predictable.
        std::vector< std::pair<std::string, std::string> > serachpairs_first(1);
        serachpairs_first[0].first = "\\\""; serachpairs_first[0].second = "";
        mujinclient::SearchAndReplace(newbuffer1, originalStr, serachpairs_first);
    }
    {
        std::vector< std::pair<std::string, std::string> > serachpairs_second(3);
        serachpairs_second[0].first = "\/"; serachpairs_second[0].second = "";
        serachpairs_second[1].first = "\\"; serachpairs_second[1].second = "";
        serachpairs_second[2].first = "\\n"; serachpairs_second[2].second = " ";
        mujinclient::SearchAndReplace(newbuffer2, newbuffer1, serachpairs_second);
    }

    std::stringstream newss;
    ConvertTimestampToFloat(newbuffer2, newss);
    boost::property_tree::read_json(newss, pt);
}

} // namespace mujinclient
