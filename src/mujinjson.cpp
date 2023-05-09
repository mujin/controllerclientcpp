// -*- coding: utf-8 -*-
// Copyright (C) 2012-2022 MUJIN Inc.
#include <mujincontrollerclient/mujinjson.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mujinjson {

template <class Container>
void ParseJsonFile(rapidjson::Document& d, const char* filename, Container& buffer)
{
    static_assert(sizeof(typename Container::value_type) == sizeof(char), "buffer must contain byte-sized elements");

    const int fd = ::open(filename, O_RDONLY);
    if (fd < 0) {
        throw MujinJSONException(boost::str(boost::format("Could not open Json file %s") % filename));
    }

    struct stat fs;
    if (::fstat(fd, &fs) != 0) {
        ::close(fd);
        throw MujinJSONException(boost::str(boost::format("Could not get file stats of Json file %s") % filename));
    }

    if( fs.st_size == 0 ) {
        // webstack files can report size 0 even though there is content in there, so have to have a growing buffer to compensate for this
        size_t nChunkSize = 4096;
        size_t nBufferSize = nChunkSize;
        char* pbuffer = (char*)::malloc(nBufferSize);
        if( !pbuffer ) {
            throw MujinJSONException("Could not allocate memory");
        }
        size_t nFileSize = 0;

        while(1) {
            size_t nTotalToRead = nBufferSize - nFileSize;
            if( nTotalToRead < nChunkSize ) {
                nBufferSize += nChunkSize;
                pbuffer = (char*)::realloc(pbuffer, nBufferSize);
                if( !pbuffer ) {
                    throw MujinJSONException("Could not allocate memory");
                }
                nTotalToRead = nBufferSize - nFileSize;
            }
            ssize_t count = ::read(fd, pbuffer + nFileSize, nBufferSize - nFileSize);
            if (count < 0) {
                if (errno == EINTR) {
                    continue; // retry if interrupted
                }
                ::close(fd);
                ::free(pbuffer);
                throw MujinJSONException(boost::str(boost::format("Could not read file data from Json file '%s'") % filename));
            }
            if( count == 0 ) {
                break; // EOF
            }
            nFileSize += count;
        }

        ::close(fd);

        if( nFileSize == 0 ) {
            ::free(pbuffer);
            throw MujinJSONException(boost::str(boost::format("JSON file '%s' is empty") % filename));
        }

        try {
            ParseJson(d, pbuffer, nFileSize);
        }
        catch(const std::exception& ex) {
            ::free(pbuffer);
            throw;
        }

        ::free(pbuffer);
        return;
    }

    buffer.resize(fs.st_size);
    ssize_t offset = 0;
    do {
        ssize_t count = ::read(fd, buffer.data() + offset, fs.st_size - offset);
        if (count < 0) {
            if (errno == EINTR) {
                continue; // retry if interrupted
            }
            ::close(fd);
            throw MujinJSONException(boost::str(boost::format("Could not read file data from Json file '%s'") % filename));
        }
        if( count == 0 ) {
            break; // EOF
        }
        offset += count;
    } while (offset < fs.st_size);

    ::close(fd);
    if( offset == 0 ) {
        throw MujinJSONException(boost::str(boost::format("JSON file '%s' is empty") % filename));
    }

    ParseJson(d, reinterpret_cast<char*>(buffer.data()), offset);
}

// used for declaration of the specialization
void __InternalParseJsonFile(rapidjson::Document& d, const char* filename)
{
    std::vector<char> buffer;
    return ParseJsonFile(d, filename, buffer);
}

} // end namespace mujinjson
