/*
 * Copyright (c) 2026 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Akshat Bharara <akshat.bharara05@gmail.com>
 *          Samved Sajankila <samvedsajankila58117@gmail.com>
 *          Sanjay S Bhat <sanjay95bhat@gmail.com>
 *          Jeferson Provalto <pravaltojeferson@gmail.com>
 *          Swaraj Singh <swaraj050605@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 *
 * Helper classes for installing QUIC client and server applications
 * on ns-3 nodes, following standard ns-3 helper conventions.
 */

#ifndef PICOQUIC_HELPER_H
#define PICOQUIC_HELPER_H

#include "ns3/application-container.h"
#include "ns3/inet-socket-address.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"

#include <cstdint>
#include <string>

namespace ns3
{
namespace picoquic_ns3
{

/**
 * @brief Helper for installing QuicServerApp on nodes.
 *
 * Usage:
 * @code
 *   QuicServerHelper server(InetSocketAddress(Ipv4Address::GetAny(), 4433));
 *   server.SetAttribute("CertFile", StringValue("/path/to/cert.pem"));
 *   server.SetAttribute("KeyFile",  StringValue("/path/to/key.pem"));
 *   server.SetAttribute("CongestionControl", StringValue("bbr"));
 *   ApplicationContainer apps = server.Install(nodes);
 *   apps.Start(Seconds(0.0));
 *   apps.Stop(Seconds(30.0));
 * @endcode
 */
class QuicServerHelper
{
  public:
    explicit QuicServerHelper(const InetSocketAddress& listenAddr);

    void SetAttribute(const std::string& name, const AttributeValue& value);

    ApplicationContainer Install(NodeContainer nodes) const;

    ApplicationContainer Install(Ptr<Node> node) const;

  private:
    ObjectFactory m_factory;
};

/**
 * @brief Helper for installing QuicClientApp on nodes.
 *
 * Usage:
 * @code
 *   QuicClientHelper client(InetSocketAddress(serverAddr, 4433));
 *   client.SetAttribute("MaxBytes", UintegerValue(1000000));
 *   client.SetAttribute("CongestionControl", StringValue("bbr"));
 *   ApplicationContainer apps = client.Install(nodes);
 *   apps.Start(Seconds(1.0));
 *   apps.Stop(Seconds(30.0));
 * @endcode
 */
class QuicClientHelper
{
  public:
    explicit QuicClientHelper(const InetSocketAddress& remoteAddr);

    void SetAttribute(const std::string& name, const AttributeValue& value);

    ApplicationContainer Install(NodeContainer nodes) const;

    ApplicationContainer Install(Ptr<Node> node) const;

  private:
    ObjectFactory m_factory;
};

} 
} 

#endif 
