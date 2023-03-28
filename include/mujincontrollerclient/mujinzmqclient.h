// -*- coding: utf-8 -*-
#ifndef MUJIN_ZMQCLIENT_H
#define MUJIN_ZMQCLIENT_H

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <mujincontrollerclient/zmq.hpp>
#include <mujincontrollerclient/mujinzmq.h>
#include <mujincontrollerclient/mujinexceptions.h>
#include <mujincontrollerclient/mujinjson.h>
#include <mujincontrollerclient/mujindefinitions.h>

namespace mujinclient {
using namespace mujinjson;

/** \brief client to mujin controller via zmq socket connection
 */
class ZmqMujinControllerClient : public mujinzmq::ZmqClient
{
public:
    ZmqMujinControllerClient(boost::shared_ptr<zmq::context_t> context, const std::string& host, const int port);

    virtual ~ZmqMujinControllerClient() = default; // _DestroySocket() is called in  ~ZmqClient()
};

typedef boost::shared_ptr<ZmqMujinControllerClient> ZmqMujinControllerClientPtr;
typedef boost::weak_ptr<ZmqMujinControllerClient> ZmqMujinControllerClientWeakPtr;

} // namespace mujinclient

#endif // #ifndef MUJIN_ZMQCLIENT_H
