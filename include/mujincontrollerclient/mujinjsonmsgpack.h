#ifndef MUJIN_CONTROLLERCLIENT_JSONMSGPACK_H
#define MUJIN_CONTROLLERCLIENT_JSONMSGPACK_H
#include "mujincontrollerclient/mujinjson.h"
#include "msgpack.hpp"
#include "mujinmasterslaveclient.h"

template<typename Encoding, typename Allocator>
struct MsgpackParser {
    /*
     * Fast parser for turning msgpack -> json
     * without intermediate msgpack_object repr.
     */

    bool visit_nil()
    {
        _objects.emplace_back(rapidjson::kNullType);
        return true;
    }

    bool visit_boolean(const bool value)
    {
        _objects.emplace_back(value);
        return true;
    }

    bool visit_positive_integer(const uint64_t value)
    {
        _objects.emplace_back(value);
        return true;
    }

    bool visit_negative_integer(const int64_t value)
    {
        _objects.emplace_back(value);
        return true;
    }

    bool visit_float32(const float value)
    {
        _objects.emplace_back(value);
        return true;
    }

    bool visit_float64(const double value)
    {
        _objects.emplace_back(value);
        return true;
    }

    bool visit_str(const char* const value, const uint32_t size)
    {
        _objects.emplace_back(value, size, _allocator);
        return true;
    }

    bool visit_bin(const char* const value, const uint32_t size)
    {
        _objects.emplace_back(value, size, _allocator);
        return true;
    }

    bool visit_ext(const char* value, const uint32_t valueSize)
    {
        msgpack::object object;
        object.type = msgpack::type::EXT;
        object.via.ext.ptr = value;
        object.via.ext.size = valueSize - 1;

        const std::chrono::system_clock::time_point tp = object.as<std::chrono::system_clock::time_point>();
        const std::time_t parsedTime = std::chrono::system_clock::to_time_t(tp);

        // RFC 3339 Nano format
        char formatted[sizeof("2006-01-02T15:04:05.999999999Z07:00")];

        // The extension does not include timezone information. By convention, we format to local time.
        tm datetime = {};
        std::size_t size = std::strftime(formatted, sizeof(formatted), "%FT%T", localtime_r(&parsedTime, &datetime));

        // Add nanoseconds portion if present
        const long nanoseconds = (std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count() % 1000000000 + 1000000000) % 1000000000;
        if (nanoseconds != 0) {
            size += sprintf(formatted + size, ".%09lu", nanoseconds);
            // remove trailing zeros
            while (formatted[size - 1] == '0') {
                --size;
            }
        }
        if (datetime.tm_gmtoff == 0) {
            formatted[size] = 'Z';
        } else {
            size += std::strftime(formatted + size, sizeof(formatted) - size, "%z", &datetime);
            // fix timezone format (0000 -> 00:00)
            formatted[size] = formatted[size - 1];
            formatted[size - 1] = formatted[size - 2];
            formatted[size - 2] = ':';
        }

        _objects.emplace_back(formatted, size + 1, _allocator);
        return true;
    }

    bool start_array(const uint32_t size)
    {
        _objects.emplace_back(rapidjson::kArrayType);
        _objects.back().Reserve(size, _allocator);
        return true;
    }

    static bool start_array_item()
    {
        return true;
    }

    bool end_array_item()
    {
        rapidjson::Value top = std::move(_objects.back());
        _objects.pop_back();

        _objects.back().PushBack(top, _allocator);
        return true;
    }

    static bool end_array()
    {
        return true;
    }

    bool start_map(const uint32_t size)
    {
        _objects.emplace_back(rapidjson::kObjectType);
        _objects.back().MemberReserve(size, _allocator);
        return true;
    }

    static bool start_map_key()
    {
        return true;
    }

    static bool end_map_key()
    {
        return true;
    }

    static bool start_map_value()
    {
        return true;
    }

    bool end_map_value()
    {
        rapidjson::Value value = std::move(_objects.back());
        _objects.pop_back();

        rapidjson::Value key = std::move(_objects.back());
        _objects.pop_back();

        _objects.back().AddMember(key, value, _allocator);
        return true;
    }

    static bool end_map()
    {
        return true;
    }

    rapidjson::Value Extract()
    {
        if (_objects.size() != 1) {
            throw msgpack::parse_error("parse error");
        }
        rapidjson::Value result = std::move(_objects.back());
        _objects.pop_back();
        return result;
    }

    static void parse_error(size_t /*parsed_offset*/, size_t /*error_offset*/)
    {
        throw msgpack::parse_error("parse error");
    }

    static void insufficient_bytes(size_t /*parsed_offset*/, size_t /*error_offset*/)
    {
        throw msgpack::insufficient_bytes("insufficient bytes");
    }

    explicit MsgpackParser(Allocator& allocator): _allocator(allocator)
    {
    }

private:
    std::vector<rapidjson::GenericValue<Encoding> > _objects;
    Allocator& _allocator;
};

namespace mujinmasterslaveclient {
template<typename Encoding, typename Allocator, typename StackAllocator>
struct MessageParser<rapidjson::GenericDocument<Encoding, Allocator, StackAllocator> > : MsgpackParser<Encoding, Allocator> {
    explicit MessageParser(rapidjson::GenericDocument<Encoding, Allocator, StackAllocator> document = {}):
        MsgpackParser<Encoding, Allocator>(document.GetAllocator()),
        _document(std::move(document))
    {
    }

    rapidjson::GenericDocument<Encoding, Allocator, StackAllocator> Extract()
    {
        MsgpackParser<Encoding, Allocator>::Extract().Swap(_document);
        return std::move(_document);
    }

private:
    rapidjson::GenericDocument<Encoding, Allocator, StackAllocator> _document;
};
}

using GenericMsgpackParser = MsgpackParser<rapidjson::Document::EncodingType, rapidjson::Document::AllocatorType>;

namespace msgpack {
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
namespace adaptor {
template<typename Encoding, typename Allocator>
struct pack<rapidjson::GenericValue<Encoding, Allocator> > {
    template<typename Stream>
    packer<Stream>& operator()(packer<Stream>& o, rapidjson::GenericValue<Encoding, Allocator> const& v) const
    {
        switch (v.GetType()) {
            case rapidjson::kNullType:
                return o.pack_nil();
            case rapidjson::kFalseType:
                return o.pack_false();
            case rapidjson::kTrueType:
                return o.pack_true();
            case rapidjson::kObjectType: {
                o.pack_map(v.MemberCount());
                typename rapidjson::GenericValue<Encoding, Allocator>::ConstMemberIterator i = v.MemberBegin(), END = v.MemberEnd();
                for (; i != END; ++i) {
                    o.pack(i->name);
                    o.pack(i->value);
                }
                return o;
            }
            case rapidjson::kArrayType: {
                o.pack_array(v.Size());
                typename rapidjson::GenericValue<Encoding, Allocator>::ConstValueIterator i = v.Begin(), END = v.End();
                for (; i < END; ++i) {
                    o.pack(*i);
                }
                return o;
            }
            case rapidjson::kStringType:
                return o.pack_str(v.GetStringLength()).pack_str_body(v.GetString(), v.GetStringLength());
            case rapidjson::kNumberType:
                if (v.IsInt())
                    return o.pack_int(v.GetInt());
                if (v.IsUint())
                    return o.pack_unsigned_int(v.GetUint());
                if (v.IsInt64())
                    return o.pack_int64(v.GetInt64());
                if (v.IsUint64())
                    return o.pack_uint64(v.GetUint64());
                if (v.IsDouble())
                    return o.pack_double(v.GetDouble());
            default:
                return o;
        }
    }
};

template<typename Encoding, typename Allocator, typename StackAllocator>
struct pack<rapidjson::GenericDocument<Encoding, Allocator, StackAllocator> > {
    template<typename Stream>
    packer<Stream>& operator()(packer<Stream>& o, rapidjson::GenericDocument<Encoding, Allocator, StackAllocator> const& v) const
    {
        return o.pack(static_cast<const rapidjson::GenericValue<Encoding, Allocator>&>(v));
    }
};
}
}
}

#endif //MUJIN_CONTROLLERCLIENT_JSONMSGPACK_H
