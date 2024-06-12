#include "mujincontrollerclient/mujinmasterslaveclient.h"
#include <boost/utility/string_view.hpp>

namespace mujinmasterslaveclient {
RequestSocket::RequestSocket(zmq::context_t& context, const std::string& address): socket_t(context, zmq::socket_type::req)
{
    // turn on tcp keepalive, do these configuration before bind
    set(zmq::sockopt::tcp_keepalive, 1);

    // the interval between the last data packet sent (simple ACKs are not considered data) and the first
    // keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
    set(zmq::sockopt::tcp_keepalive_idle, 2);

    // the interval between subsequent keepalive probes, regardless of what the connection
    // has exchanged in the meantime
    set(zmq::sockopt::tcp_keepalive_intvl, 2);

    // the number of unacknowledged probes to send before considering the connection dead
    // and notifying the application layer
    set(zmq::sockopt::tcp_keepalive_cnt, 2);

    connect(address);
}

void RequestSocket::SendNoWait(std::vector<zmq::message_t>&& messages)
{
    for (std::vector<zmq::message_t>::iterator iterator = messages.begin(); iterator != messages.end(); ++iterator) {
        if (!send(*iterator, iterator + 1 == messages.end() ? zmq::send_flags::dontwait : (zmq::send_flags::sndmore | zmq::send_flags::dontwait)).has_value()) {
            throw mujinclient::MujinException("unable to send zmq message", mujinclient::MEC_InvalidState);
        }
    }
}

std::vector<zmq::message_t> RequestSocket::ReceiveNoWait()
{
    std::vector<zmq::message_t> frames;
    bool hasMore = true;
    while (hasMore) {
        frames.emplace_back();
        zmq::message_t& message = frames.back();
        if (!recv(message, zmq::recv_flags::dontwait).has_value()) {
            throw mujinclient::MujinException("unable to receive zmq message", mujinclient::MEC_InvalidState);
        }
        hasMore = message.more();
    }
    return frames;
}

bool RequestSocket::Poll(const short events, const std::chrono::milliseconds timeout)
{
    zmq::pollitem_t item = {
        .socket = handle(),
        .events = events,
    };
    return zmq::poll(&item, 1, timeout);
}

std::vector<zmq::message_t> RequestSocket::SendAndReceive(
    std::vector<zmq::message_t>&& messages,
    std::chrono::milliseconds timeout)
{
    if (messages.empty()) {
        throw mujinclient::MujinException("given messages is empty", mujinclient::MEC_InvalidArguments);
    }

    if (timeout.count() < 0) {
        throw mujinclient::MujinException("timeout cannot be negative", mujinclient::MEC_InvalidArguments);
    }

    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout;

    if (!Poll(ZMQ_POLLOUT, timeout)) {
        throw mujinclient::MujinException("timeout trying to send request", mujinclient::MEC_Timeout);
    }
    SendNoWait(std::move(messages));

    timeout = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if (timeout.count() < 0) {
        throw mujinclient::MujinException("timeout trying to send request", mujinclient::MEC_Timeout);
    }

    if (!Poll(ZMQ_POLLIN, timeout)) {
        throw mujinclient::MujinException("timeout trying to receive response", mujinclient::MEC_Timeout);
    }
    std::vector<zmq::message_t> response = ReceiveNoWait();
    assert(!response.empty()); // never going to happen

    const boost::string_view statusFrame = boost::string_view(response.front().data<char>(), response.front().size());

    if (statusFrame == "f") {
        if (response.size() != 2) {
            throw mujinclient::MujinException("unexpcted number of frames in error", mujinclient::MEC_InvalidState);
        }
        throw std::logic_error(response.back().to_string());
    }

    if (statusFrame != "t") {
        throw mujinclient::MujinException("unexpected response protocol", mujinclient::MEC_InvalidState);
    }

    response.erase(response.begin());
    return response;
}
}
