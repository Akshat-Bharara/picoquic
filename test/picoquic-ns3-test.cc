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
 * Test suite for the picoquic-ns3 integration module.
 * Verifies that the QUIC context, connection, and transport
 * components work correctly within the ns-3 simulation environment.
 */

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

#include "ns3/picoquic-client-app.h"
#include "ns3/picoquic-context.h"
#include "ns3/picoquic-helper.h"
#include "ns3/picoquic-server-app.h"
#include "ns3/picoquic-sim-time.h"
#include "ns3/picoquic-transport.h"

#include <picoquic.h>
 
using namespace ns3;
using namespace ns3::picoquic_ns3;

class SimTimeBridgeTestCase : public TestCase
 {
   public:
     SimTimeBridgeTestCase()
         : TestCase("SimulatedTimeBridge synchronisation")
     {
     }
 
   private:
     void DoRun() override
     {
         Simulator::Destroy();
 
        auto& bridge = SimulatedTimeBridge::GetInstance();

        bridge.Update();
        NS_TEST_ASSERT_MSG_EQ(bridge.GetCurrentTimeUs(), 0, "Time should be 0 at start");

        Simulator::Schedule(MilliSeconds(500), [&bridge, this]() {
            bridge.Update();
            NS_TEST_ASSERT_MSG_EQ(bridge.GetCurrentTimeUs(),
                                  500000,
                                  "Time should be 500000 µs at 500 ms");
        });

        Simulator::Schedule(Seconds(1), [&bridge, this]() {
             bridge.Update();
             NS_TEST_ASSERT_MSG_EQ(bridge.GetCurrentTimeUs(),
                                   1000000,
                                   "Time should be 1000000 µs at 1 s");
         });
 
         Simulator::Stop(Seconds(2));
         Simulator::Run();
         Simulator::Destroy();
     }
};

class QuicContextTestCase : public TestCase
 {
   public:
     QuicContextTestCase()
         : TestCase("QuicContext create and destroy")
     {
     }
 
   private:
     void DoRun() override
     {
         Simulator::Destroy();
 
         QuicContextConfig cfg;
         cfg.maxConnections = 16;
         cfg.defaultAlpn = "h3";
         cfg.disableCertVerification = true;
         cfg.ccAlgorithm = "newreno";
         cfg.idleTimeoutMs = 30000;
 
        {
            QuicContext ctx(cfg);
            NS_TEST_ASSERT_MSG_NE(ctx.GetNative(), nullptr, "Context should not be null");
            NS_TEST_ASSERT_MSG_EQ(static_cast<bool>(ctx), true, "Context should be valid");
        }
 
         Simulator::Destroy();
     }
};

class QuicE2eTestCase : public TestCase
 {
   public:
     QuicE2eTestCase()
         : TestCase("QUIC end-to-end transfer"),
           m_serverBytesRx(0),
           m_clientConnected(false)
     {
     }
 
   private:
     void DoRun() override;
     void ServerRxCallback(Ptr<const QuicServerApp>, uint64_t bytes);
     void ClientConnectedCallback(Ptr<const QuicClientApp>);
 
     uint64_t m_serverBytesRx;
     bool m_clientConnected;
 };
 
 void
 QuicE2eTestCase::ServerRxCallback(Ptr<const QuicServerApp>, uint64_t bytes)
 {
     m_serverBytesRx += bytes;
 }
 
 void
 QuicE2eTestCase::ClientConnectedCallback(Ptr<const QuicClientApp>)
 {
     m_clientConnected = true;
 }
 
 void
 QuicE2eTestCase::DoRun()
 {
     Simulator::Destroy();
 
    /* Topology: n0 <-- p2p 10 Mbps, 5 ms --> n1 */
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));
    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper addr;
    addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = addr.Assign(devices);

    /* picoquic requires TLS certs even in simulation */
    InetSocketAddress serverAddr(ifaces.GetAddress(1), 4433);
 
     /* PICOQUIC_NS3_PICOQUIC_PREFIX is injected by CMakeLists.txt as a
      * compile definition pointing to the picoquic source root. */
 #ifndef PICOQUIC_NS3_PICOQUIC_PREFIX
 #define PICOQUIC_NS3_PICOQUIC_PREFIX ""
 #endif
     const std::string picoquicRoot = std::string(PICOQUIC_NS3_PICOQUIC_PREFIX);
     const std::string certFile = picoquicRoot + "/certs/cert.pem";
     const std::string keyFile = picoquicRoot + "/certs/key.pem";
     const std::string caFile = picoquicRoot + "/certs/test-ca.crt";
 
     QuicServerHelper serverHelper(serverAddr);
     serverHelper.SetAttribute("CertFile", StringValue(certFile));
     serverHelper.SetAttribute("KeyFile", StringValue(keyFile));
     ApplicationContainer serverApps = serverHelper.Install(nodes.Get(1));
     serverApps.Start(Seconds(0.0));
     serverApps.Stop(Seconds(20.0));
 
     /* Connect trace */
     Ptr<QuicServerApp> serverApp = DynamicCast<QuicServerApp>(serverApps.Get(0));
     serverApp->m_rxTrace.ConnectWithoutContext(
         MakeCallback(&QuicE2eTestCase::ServerRxCallback, this));
 
     /* Client on n0 — send 10000 bytes */
     const uint64_t transferSize = 10000;
     QuicClientHelper clientHelper(serverAddr);
     clientHelper.SetAttribute("MaxBytes", UintegerValue(transferSize));
     clientHelper.SetAttribute("Sni", StringValue("test.example.com"));
     if (!caFile.empty())
     {
         clientHelper.SetAttribute("CertRootFile", StringValue(caFile));
     }
     ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
     clientApps.Start(Seconds(1.0));
     clientApps.Stop(Seconds(20.0));
 
     Ptr<QuicClientApp> clientApp = DynamicCast<QuicClientApp>(clientApps.Get(0));
     clientApp->m_connectionEstablished.ConnectWithoutContext(
         MakeCallback(&QuicE2eTestCase::ClientConnectedCallback, this));
 
     Simulator::Stop(Seconds(20.0));
     Simulator::Run();
 
     NS_TEST_ASSERT_MSG_EQ(m_clientConnected, true, "Client should have established a connection");
 
     NS_TEST_ASSERT_MSG_EQ(m_serverBytesRx, transferSize, "Server should have received all bytes");
 
     Simulator::Destroy();
}

class PicoquicNs3TestSuite : public TestSuite
{
  public:
    PicoquicNs3TestSuite()
        : TestSuite("picoquic-ns3", Type::UNIT)
    {
        AddTestCase(new SimTimeBridgeTestCase, TestCase::Duration::QUICK);
        AddTestCase(new QuicContextTestCase, TestCase::Duration::QUICK);
        AddTestCase(new QuicE2eTestCase, TestCase::Duration::EXTENSIVE);
    }
};

static PicoquicNs3TestSuite g_picoquicNs3TestSuite;
 