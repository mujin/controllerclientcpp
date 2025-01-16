#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/beast.hpp>
#include <boost/thread/mutex.hpp>

namespace mujinclient {

struct CookieDecorator {
    std::string csrftoken;
    std::string encodedusernamepassword;

    void operator()(boost::beast::websocket::request_type& req) const {
        req.set(boost::beast::http::field::cookie, "csrftoken=" + csrftoken);
        req.set(boost::beast::http::field::authorization, "Basic " + encodedusernamepassword);
    }
};

class BaseWebSocketStream {
public:
    virtual ~BaseWebSocketStream() = default;
    virtual bool is_open() = 0;
    virtual void set_option(const std::string& csrftoken, const std::string& encodedusernamepassword) = 0;
    virtual void write(const std::string& writeMessage) = 0;
    virtual void handshake(const std::string& host, const std::string& target) = 0;
    virtual std::string read() = 0;
    virtual void close() = 0;
};

class TCPWebSocketStream : public BaseWebSocketStream {
public:
    TCPWebSocketStream(boost::asio::ip::tcp::socket&& socket) : _stream(std::move(socket)){};
    ~TCPWebSocketStream();
    bool is_open();
    void set_option(const std::string& csrftoken, const std::string& encodedusernamepassword);
    void write(const std::string& writeMessage);
    void handshake(const std::string& host, const std::string& target);
    std::string read();
    void close();

private:
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> _stream;
    boost::beast::flat_buffer _streamBuffer;
    boost::mutex _mutex;
};

class UnixWebSocketStream : public BaseWebSocketStream {
public:
    UnixWebSocketStream(boost::asio::local::stream_protocol::socket&& socket) : _stream(std::move(socket)){};
    ~UnixWebSocketStream();
    bool is_open();
    void set_option(const std::string& csrftoken, const std::string& encodedusernamepassword);
    void write(const std::string& writeMessage);
    void handshake(const std::string& host, const std::string& target);
    std::string read();
    void close();

private:
    boost::beast::websocket::stream<boost::asio::local::stream_protocol::socket> _stream;
    boost::beast::flat_buffer _streamBuffer;
    boost::mutex _mutex;
};

} // namespace mujinclient