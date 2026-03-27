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
 */

#include "picoquic-helper.h"

#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/string.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PicoquicHelper");

namespace picoquic_ns3
{

/* ================================================================== */
/*  QuicServerHelper                                                  */
/* ================================================================== */

QuicServerHelper::QuicServerHelper(const InetSocketAddress& listenAddr)
{
    m_factory.SetTypeId("ns3::picoquic_ns3::QuicServerApp");
    m_factory.Set("ListenAddress", AddressValue(listenAddr));
}

void
QuicServerHelper::SetAttribute(const std::string& name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
QuicServerHelper::Install(NodeContainer nodes) const
{
    ApplicationContainer apps;
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        apps.Add(Install(*it));
    }
    return apps;
}

ApplicationContainer
QuicServerHelper::Install(Ptr<Node> node) const
{
    Ptr<Application> app = m_factory.Create<Application>();
    node->AddApplication(app);
    return ApplicationContainer(app);
}

/* ================================================================== */
/*  QuicClientHelper                                                  */
/* ================================================================== */

QuicClientHelper::QuicClientHelper(const InetSocketAddress& remoteAddr)
{
    m_factory.SetTypeId("ns3::picoquic_ns3::QuicClientApp");
    m_factory.Set("RemoteAddress", AddressValue(remoteAddr));
}

void
QuicClientHelper::SetAttribute(const std::string& name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
QuicClientHelper::Install(NodeContainer nodes) const
{
    ApplicationContainer apps;
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        apps.Add(Install(*it));
    }
    return apps;
}

ApplicationContainer
QuicClientHelper::Install(Ptr<Node> node) const
{
    Ptr<Application> app = m_factory.Create<Application>();
    node->AddApplication(app);
    return ApplicationContainer(app);
}

} 
} 
