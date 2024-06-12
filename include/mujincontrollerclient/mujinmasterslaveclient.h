#ifndef MUJIN_MASTERSLAVECLIENT_REQUESTSOCKET_H
#define MUJIN_MASTERSLAVECLIENT_REQUESTSOCKET_H
#include <mujincontrollerclient/zmq.hpp>
#include <mujincontrollerclient/mujinexceptions.h>
#include <msgpack.hpp>

namespace mujinmasterslaveclient {
template<typename ValueType>
struct MessageParser;

template<typename ValueType>
static zmq::message_t EncodeToFrame(const ValueType& value)
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, value);
    return {buffer.data(), buffer.size()};
}

template<typename ValueType>
static std::vector<zmq::message_t> EncodeToMessage(const ValueType& value)
{
    std::vector<zmq::message_t> messages;
    messages.emplace_back(EncodeToFrame(value));
    return messages;
}

template<typename ValueType>
static ValueType DecodeFromFrame(const zmq::message_t& frame)
{
    MessageParser<ValueType> parser;
    if (!msgpack::parse(frame.data<char>(), frame.size(), parser)) {
        throw std::invalid_argument("unable to parse");
    }
    return parser.Extract();
}

struct RequestSocket : private zmq::socket_t {
    RequestSocket(zmq::context_t& context, const std::string& address);

    void SendNoWait(std::vector<zmq::message_t>&& messages);

    std::vector<zmq::message_t> ReceiveNoWait();

    [[nodiscard]] bool Poll(short events, std::chrono::milliseconds timeout);

    std::vector<zmq::message_t> SendAndReceive(std::vector<zmq::message_t>&& messages, std::chrono::milliseconds timeout);
};

template<typename InputType, typename OutputType = InputType>
static OutputType SendAndReceive(RequestSocket& socket, const InputType& master, const std::chrono::milliseconds timeout)
{
    std::vector<zmq::message_t> frames;
    frames.emplace_back(EncodeToFrame(master));
    const std::vector<zmq::message_t> response = socket.SendAndReceive(std::move(frames), timeout);
    if (response.size() != 1) {
        throw mujinclient::MujinException("unexpected server response protocol", mujinclient::MEC_InvalidState);
    }
    return DecodeFromFrame<OutputType>(response.front());
}

template<typename InputType, typename OutputType = InputType>
static OutputType SendAndReceive(RequestSocket& socket, const InputType& master, const InputType& slave, const std::chrono::milliseconds timeout)
{
    std::vector<zmq::message_t> frames;
    frames.emplace_back(EncodeToFrame(master));
    frames.emplace_back(EncodeToFrame(slave));
    const std::vector<zmq::message_t> response = socket.SendAndReceive(std::move(frames), timeout);
    if (response.size() != 1) {
        throw mujinclient::MujinException("unexpected server response protocol", mujinclient::MEC_InvalidState);
    }
    return DecodeFromFrame<OutputType>(response.front());
}

template<typename InputType, typename Parser>
static bool SendAndReceive(RequestSocket& socket, const InputType& master, Parser& parser, const std::chrono::milliseconds timeout)
{
    std::vector<zmq::message_t> frames;
    frames.emplace_back(EncodeToFrame(master));
    const std::vector<zmq::message_t> response = socket.SendAndReceive(std::move(frames), timeout);
    if (response.size() != 1) {
        throw mujinclient::MujinException("unexpected server response protocol", mujinclient::MEC_InvalidState);
    }
    const zmq::message_t& frame = response.front();
    return msgpack::parse(frame.data<char>(), frame.size(), parser);
}


template<typename InputType, typename Parser>
static bool SendAndReceive(RequestSocket& socket, const InputType& master, const InputType& slave, Parser& parser, const std::chrono::milliseconds timeout)
{
    std::vector<zmq::message_t> frames;
    frames.emplace_back(EncodeToFrame(master));
    frames.emplace_back(EncodeToFrame(slave));
    const std::vector<zmq::message_t> response = socket.SendAndReceive(std::move(frames), timeout);
    if (response.size() != 1) {
        throw mujinclient::MujinException("unexpected server response protocol", mujinclient::MEC_InvalidState);
    }
    const zmq::message_t& frame = response.front();
    return msgpack::parse(frame.data<char>(), frame.size(), parser);
}
}

#endif //MUJIN_MASTERSLAVECLIENT_REQUESTSOCKET_H
