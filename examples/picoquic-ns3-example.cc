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
 * Example: point-to-point QUIC bulk transfer using the picoquic-ns3
 * integration module.
 *
 * Topology:
 *
 *   n0 (client) ---- point-to-point (10 Mbps, 20 ms) ---- n1 (server)
 *   10.1.1.1                                                10.1.1.2
 *
 * The client sends a configurable amount of data to the server over a
 * single QUIC stream.  Results report:
 *   - Streams opened, upload bytes, server-received bytes
 *   - Transfer time (handshake completion → last byte enqueued)
 *   - Upload Mbps (client-side goodput)
 *   - Supplementary: handshake time, connection close time, wire throughput
 *
 * NOTE on "Transfer time": measured from handshake completion to the moment
 * the last byte is submitted to the stream (FIN enqueued), not when the
 * server acknowledges receipt.  For end-to-end round-trip measurement use
 * picoquic-ns3-quicperf with a matching post_size:response_size scenario.
 *
 * =====================================================================
 * COMMAND-LINE PARAMETERS
 * =====================================================================
 *
 *   --maxBytes   Total bytes for the client to send  (default: 1000000)
 *   --ccAlgo     Congestion control: bbr | cubic | newreno  (default: bbr)
 *   --simTime    Simulation duration in seconds  (default: 30.0)
 *   --qlogDir    Directory for QLOG output; empty = disabled  (default: "")
 *   --pcap       Enable PCAP capture on both nodes  (default: false)
 *
 * =====================================================================
 * USAGE EXAMPLES
 * =====================================================================
 *
 * 1. Run with all defaults (1 MB upload, BBR, 10 Mbps / 20 ms):
 *
 *      ./ns3 run picoquic-ns3-example
 *
 * 2. Change the amount of data to transfer:
 *
 *      ./ns3 run "picoquic-ns3-example --maxBytes=5000000"
 *
 * 3. Change the congestion control algorithm:
 *
 *      ./ns3 run "picoquic-ns3-example --ccAlgo=cubic"
 *      ./ns3 run "picoquic-ns3-example --ccAlgo=newreno"
 *
 * 4. Enable QLOG output (one .qlog file per endpoint):
 *
 *      ./ns3 run "picoquic-ns3-example --qlogDir=/tmp/qlogs"
 *
 * 5. Enable PCAP capture:
 *
 *      ./ns3 run "picoquic-ns3-example --pcap=true"
 *
 *    Produces: picoquic-ns3-example-0-0.pcap  (client)
 *              picoquic-ns3-example-1-0.pcap  (server)
 *
 * 6. Combine multiple options:
 *
 *      ./ns3 run "picoquic-ns3-example --maxBytes=5000000 --ccAlgo=cubic \
 *                 --simTime=20 --pcap=true"
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

/* picoquic-ns3 headers */
#include "ns3/picoquic-client-app.h"
#include "ns3/picoquic-helper.h"
#include "ns3/picoquic-server-app.h"

using namespace ns3;
using namespace ns3::picoquic_ns3;

NS_LOG_COMPONENT_DEFINE("PicoquicExample");

int
main(int argc, char* argv[])
{
    uint64_t maxBytes = 1000000; 
    std::string ccAlgo = "bbr";
    double simTimeSec = 30.0;
    std::string qlogDir = ""; /* empty = disabled; e.g. "/tmp/qlogs" */
    bool pcapEnabled = false; /* pass --pcap to enable */

    CommandLine cmd(__FILE__);
    cmd.AddValue("maxBytes", "Total bytes to transfer", maxBytes);
    cmd.AddValue("ccAlgo", "Congestion control (bbr, cubic, newreno)", ccAlgo);
    cmd.AddValue("simTime", "Simulation duration in seconds", simTimeSec);
    cmd.AddValue("qlogDir", "Directory for QLOG output (empty = disabled)", qlogDir);
    cmd.AddValue("pcap", "Enable PCAP capture on both nodes", pcapEnabled);
    cmd.Parse(argc, argv);

    /* ------------------------------------------------------------ */
    /*  Topology: two nodes with a point-to-point link               */
    /* ------------------------------------------------------------ */
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("20ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    /* ------------------------------------------------------------ */
    /*  Statistics counters                                         */
    /* ------------------------------------------------------------ */
    uint64_t clientBytesSent = 0;
    uint64_t serverBytesRx = 0;
    uint64_t wireBytesTx = 0; ///< All bytes on the wire from node 0 (incl. headers)
    uint64_t streamsOpened = 0;
    bool clientConnected = false;
    Time connectionTime;
    Time completionTime; ///< When the last byte was submitted to the stream (FIN)
    Time closedTime;     ///< When the connection was fully closed

    /* ------------------------------------------------------------ */
    /*  QUIC server on n1 (10.1.1.2 : 4433)                        */
    /* ------------------------------------------------------------ */
    InetSocketAddress serverAddr(interfaces.GetAddress(1), 4433);

    /* Wire-level Tx counter on node 0's device — counts every byte
     * physically transmitted (Ethernet framing + IP + UDP + QUIC) */
    devices.Get(0)->TraceConnectWithoutContext(
        "PhyTxEnd",
        MakeBoundCallback(
            +[](uint64_t* counter, Ptr<const Packet> pkt) { *counter += pkt->GetSize(); },
            &wireBytesTx));

    QuicServerHelper server(serverAddr);
    server.SetAttribute("Alpn", StringValue("h3"));
    server.SetAttribute("CongestionControl", StringValue(ccAlgo));
    /* Uncomment to enable QLOG on the server (or pass --qlogDir=/tmp/qlogs): */
    // server.SetAttribute("QlogDir", StringValue("/tmp/qlogs"));
    if (!qlogDir.empty())
    {
        server.SetAttribute("QlogDir", StringValue(qlogDir));
    }

    /* Use picoquic's bundled test certificates for TLS.  The path is
     * set via a compile-time macro from CMakeLists.txt. */
#ifdef PICOQUIC_NS3_PICOQUIC_PREFIX
    server.SetAttribute("CertFile",
                        StringValue(std::string(PICOQUIC_NS3_PICOQUIC_PREFIX) + "/certs/cert.pem"));
    server.SetAttribute("KeyFile",
                        StringValue(std::string(PICOQUIC_NS3_PICOQUIC_PREFIX) + "/certs/key.pem"));
#endif

    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTimeSec));

    Ptr<QuicServerApp> serverApp = DynamicCast<QuicServerApp>(serverApps.Get(0));
    serverApp->m_rxTrace.ConnectWithoutContext(MakeBoundCallback(
        +[](uint64_t* counter, Ptr<const QuicServerApp>, uint64_t bytes) { *counter += bytes; },
        &serverBytesRx));
    serverApp->m_connectionAccepted.ConnectWithoutContext(MakeBoundCallback(
        +[](uint64_t* counter, Ptr<const QuicServerApp>) { (*counter)++; },
        &streamsOpened));

    /* ------------------------------------------------------------ */
    /*  QUIC client on n0                                           */
    /* ------------------------------------------------------------ */
    QuicClientHelper client(serverAddr);
    client.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    client.SetAttribute("CongestionControl", StringValue(ccAlgo));
    client.SetAttribute("Alpn", StringValue("h3"));
    client.SetAttribute("Sni", StringValue("test.example.com"));
    /* Uncomment to enable QLOG on the client (or pass --qlogDir=/tmp/qlogs): */
    // client.SetAttribute("QlogDir", StringValue("/tmp/qlogs"));
    if (!qlogDir.empty())
    {
        client.SetAttribute("QlogDir", StringValue(qlogDir));
    }
/* If picoquic prefix is available, point the client at the test CA so TLS
 * verification succeeds in the example environment. */
#ifdef PICOQUIC_NS3_PICOQUIC_PREFIX
    client.SetAttribute(
        "CertRootFile",
        StringValue(std::string(PICOQUIC_NS3_PICOQUIC_PREFIX) + "/certs/test-ca.crt"));
#endif

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTimeSec));

    /* Trace client bytes sent, connection established, and completion. */
    Ptr<QuicClientApp> clientApp = DynamicCast<QuicClientApp>(clientApps.Get(0));

    clientApp->m_connectionEstablished.ConnectWithoutContext(MakeBoundCallback(
        +[](bool* connected, Time* connTime, Ptr<const QuicClientApp>) {
            *connected = true;
            *connTime = Simulator::Now();
        },
        &clientConnected,
        &connectionTime));

    clientApp->m_connectionClosed.ConnectWithoutContext(MakeBoundCallback(
        +[](Time* done, Ptr<const QuicClientApp>) { *done = Simulator::Now(); },
        &closedTime));

    clientApp->m_transferComplete.ConnectWithoutContext(MakeBoundCallback(
        +[](Time* done, Ptr<const QuicClientApp>) { *done = Simulator::Now(); },
        &completionTime));

    clientApp->m_txTrace.ConnectWithoutContext(MakeBoundCallback(
        +[](uint64_t* counter, Ptr<const QuicClientApp>, uint64_t bytes) { *counter += bytes; },
        &clientBytesSent));

    /* ------------------------------------------------------------ */
    /*  Optional: PCAP capture                                      */
    /*  Writes picoquic-ns3-example-<node>-<dev>.pcap in the cwd.  */
    /*  Open with: wireshark picoquic-ns3-example-0-0.pcap          */
    /*  Pass --pcap on the command line, or uncomment the line      */
    /*  below to always enable it.                                  */
    /* ------------------------------------------------------------ */
    // p2p.EnablePcapAll("picoquic-ns3-example");
    if (pcapEnabled)
    {
        p2p.EnablePcapAll("picoquic-ns3-example");
    }
    Simulator::Stop(Seconds(simTimeSec));
    Simulator::Run();
    Simulator::Destroy();

    double transferDurationSec = (completionTime - connectionTime).GetSeconds();
    double uploadMbps =
        (transferDurationSec > 0.0)
            ? (static_cast<double>(clientBytesSent) * 8.0 / transferDurationSec / 1e6)
            : 0.0;
    double downloadMbps =
        (transferDurationSec > 0.0)
            ? (static_cast<double>(serverBytesRx) * 8.0 / transferDurationSec / 1e6)
            : 0.0;

    std::cout << "\n"
              << "======= picoquic-ns3 example results  =======\n"
              << "  Topology      : " << "10Mbps, 20ms one-way delay\n"
              << "  CC algorithm  : " << ccAlgo << "\n"
              << "  Scenario      : " << maxBytes << " bytes upload\n"
              << "  Streams opened: " << streamsOpened << "\n"
              << "  Upload bytes  : " << clientBytesSent << "\n"
              << "  Server rx     : " << serverBytesRx << "\n"
              << "  Transfer time : " << transferDurationSec << " s\n"
              << "  Upload Mbps   : " << uploadMbps << "\n"
              << "  Server rx Mbps: " << downloadMbps << "\n"
              << "=============================================\n\n";

    return 0;
}
