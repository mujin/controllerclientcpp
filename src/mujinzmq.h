#ifndef MUJIN_ZMQ_H
#define MUJIN_ZMQ_H

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "zmq.hpp"
#include <sstream>

namespace mujinzmq
{

class ZmqSubscriber
{
public:
    ZmqSubscriber(std::string host, int port)
    {
        _host = host;
        _port = port;
    }

    virtual ~ZmqSubscriber()
    {
        _DestroySocket();
    }

protected:
    void _InitializeSocket()
    {
        _context.reset(new zmq::context_t (1));
        _socket.reset(new zmq::socket_t ((*_context.get()), ZMQ_SUB));

        std::ostringstream port_stream;
        port_stream << _port;
        _socket->connect (("tcp://" + _host + ":" + port_stream.str()).c_str());
        _socket->setsockopt(ZMQ_SUBSCRIBE, "", 0);
    }

    void _DestroySocket()
    {
        if (!!_socket) {
            _socket->close();
            _socket.reset();
        }
        if( !!_context ) {
            _context->close();
            _context.reset();
        }
    }

    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    std::string _host;
    int _port;
};

class ZmqPublisher
{
public:
    ZmqPublisher(int port)
    {
        _port = port;
    }

    virtual ~ZmqPublisher()
    {
        _DestroySocket();
    }

protected:
    void _InitializeSocket()
    {
        _context.reset(new zmq::context_t (1));
        _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_PUB));
        std::ostringstream port_stream;
        port_stream << _port;
        _socket->bind (("tcp://*:" + port_stream.str()).c_str());
    }

    void _DestroySocket()
    {
        if (!!_socket) {
            _socket->close();
            _socket.reset();
        }
        if( !!_context ) {
            _context->close();
            _context.reset();
        }
    }

    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
    int _port;
};

class ZmqHeartBeatPublisher : public ZmqPublisher
{
public:
    ZmqHeartBeatPublisher(int port) : ZmqPublisher(port)
    {
        _heartbeat_shutdown = false;
    }

    virtual ~ZmqHeartBeatPublisher()
    {
        StopHeartBeatTask();
    }

    virtual void StartHeartBeatTask()
    {
        _heartbeat_shutdown = false;
        _heartbeat_thread.reset(new boost::thread(boost::bind(&ZmqHeartBeatPublisher::PublishHeartBeat, this)));
    }

    virtual void StopHeartBeatTask()
    {
        _heartbeat_shutdown = true;
        if( !!_heartbeat_thread ) {
            _heartbeat_thread->join();
            _heartbeat_thread.reset();
        }
    }

    void PublishHeartBeat()
    {
        try {
            _InitializeSocket();
            while(!_heartbeat_shutdown) { // need mutex?
                //Heart Beat Packet is {255}
                zmq::message_t m(1);
                memset(m.data(), 255, 1);
                _socket->send(m);
                boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            }
            //std::cout << "PublishHeartBeat terminated safely" << std::endl;
            _DestroySocket();
        }
        catch(...) {
            //std::cerr << "PublishHeartBeat exception thrown, destroying ZMQ Socket" << std::endl;
            _DestroySocket();
            throw;
        }
    }

private:
    boost::shared_ptr<boost::thread> _heartbeat_thread;
    bool _heartbeat_shutdown;
};

class ZmqClient
{
public:
    ZmqClient(std::string host, int port)
    {
        _host = host;
        _port = port;
    }

    virtual ~ZmqClient()
    {
        _DestroySocket();
    }

protected:
    void _InitializeSocket()
    {
        _context.reset(new zmq::context_t (1));
        _socket.reset(new zmq::socket_t ((*(zmq::context_t*)_context.get()), ZMQ_REQ));
        std::ostringstream port_stream;
        port_stream << _port;
        std::cout << "connecting to socket at " << _host << ":" << _port << std::endl;
        _socket->connect (("tcp://" + _host + ":" + port_stream.str()).c_str());
    }

    void _DestroySocket()
    {
        if (!!_socket) {
            _socket->close();
            _socket.reset();
        }
        if( !!_context ) {
            _context->close();
            _context.reset();
        }
    }

    int _port;
    std::string _host;
    boost::shared_ptr<zmq::context_t> _context;
    boost::shared_ptr<zmq::socket_t>  _socket;
};

}
#endif
