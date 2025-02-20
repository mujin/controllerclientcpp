#include "websocketstream.h"
#include "logging.h"
#include <boost/format.hpp>

namespace mujinclient {

bool TCPWebSocketStream::is_open() {
    boost::mutex::scoped_lock lock(_mutex);
    return _stream.is_open();
}

void TCPWebSocketStream::set_option(const std::string& csrftoken, const std::string& encodedusernamepassword) {
    boost::mutex::scoped_lock lock(_mutex);
    CookieDecorator cookieDecorator{csrftoken, encodedusernamepassword};
    _stream.set_option(boost::beast::websocket::stream_base::decorator(cookieDecorator));
}

void TCPWebSocketStream::write(const std::string& writeMessage) {
    boost::mutex::scoped_lock lock(_mutex);
    _stream.write(boost::asio::buffer(writeMessage));
}

void TCPWebSocketStream::handshake(const std::string& host, const std::string& target) {
    boost::mutex::scoped_lock lock(_mutex);
    _stream.handshake(host, target);
}

std::string TCPWebSocketStream::read() {
    boost::mutex::scoped_lock lock(_mutex);
    _stream.read(_streamBuffer);
    std::string message = boost::beast::buffers_to_string(_streamBuffer.data());
    _streamBuffer.clear();
    return message;
}

void TCPWebSocketStream::close() {
    boost::asio::ip::tcp::socket& socket = _stream.next_layer();
    _stream.close(boost::beast::websocket::close_code::normal);
    boost::system::error_code ec;
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    /* if (ec && ec != boost::asio::error::not_connected) {
    	MUJIN_LOG_ERROR((boost::format("Error during TCP socket shutdown: %s") % ec.message()).str());
    }*/
}

TCPWebSocketStream::~TCPWebSocketStream() {
    if (is_open()) {
        close();
    }
}

bool UnixWebSocketStream::is_open() {
    boost::mutex::scoped_lock lock(_mutex);
    return _stream.is_open();
}

void UnixWebSocketStream::set_option(const std::string& csrftoken, const std::string& encodedusernamepassword) {
    boost::mutex::scoped_lock lock(_mutex);
    CookieDecorator cookieDecorator{csrftoken, encodedusernamepassword};
    _stream.set_option(boost::beast::websocket::stream_base::decorator(cookieDecorator));
}

void UnixWebSocketStream::write(const std::string& writeMessage) {
    boost::mutex::scoped_lock lock(_mutex);
    _stream.write(boost::asio::buffer(writeMessage));
}

void UnixWebSocketStream::handshake(const std::string& host, const std::string& target) {
    boost::mutex::scoped_lock lock(_mutex);
    _stream.handshake(host, target);
}

std::string UnixWebSocketStream::read() {
    boost::mutex::scoped_lock lock(_mutex);
    _stream.read(_streamBuffer);
    std::string message = boost::beast::buffers_to_string(_streamBuffer.data());
    _streamBuffer.clear();
    return message;
}

void UnixWebSocketStream::close() {
    boost::asio::local::stream_protocol::socket& socket = _stream.next_layer();
    _stream.close(boost::beast::websocket::close_code::normal);
    boost::system::error_code ec;
    socket.close(ec);
    /* if (ec) {
    	MUJIN_LOG_ERROR((boost::format("Error during unix socket shutdown: %s") % ec.message()).str());
    }*/
}

UnixWebSocketStream::~UnixWebSocketStream() {
    if (is_open()) {
        close();
    }
}
} // namespace mujinclient