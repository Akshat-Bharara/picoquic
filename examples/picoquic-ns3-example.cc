/*
 * Copyright (c) 2026 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Example: QUIC Congestion Control Evaluation (picoquic-ns3).
 *
 * This program simulates the following topology:
 *
 *           1000 Mbps           10Mbps          1000 Mbps
 *  Sender -------------- R1 -------------- R2 -------------- Receiver
 *              5ms               10ms               5ms
 *
 * The link between R1 and R2 is a bottleneck link with 10 Mbps. All other
 * links are 1000 Mbps.
 *
 * This program runs for 100 seconds and creates a new directory
 * called 'picoquic-results/<ccAlgo>-<time>' in the ns-3 root directory.
 *
 * It generates:
 * (1) PCAP files (if enabled)
 * (2) cwnd.dat - traces the QUIC sender's Congestion Window
 * (3) throughput.dat - traces the QUIC sender's throughput in Mbps
 * (4) queueSize.dat - traces the queue length at the bottleneck link
 * (5) QLOG files (if enabled via --enableQlog)
 * 
 * =====================================================================
 * USAGE EXAMPLES
 * =====================================================================
 *
 * 1. Run with Cubic (defaults) for 100 seconds:
 *      ./ns3 run picoquic-ns3-example
 *
 * 2. Run with BBR and generate QLOG files:
 *      ./ns3 run "picoquic-ns3-example --ccAlgo=bbr --enableQlog=1"
 *
 * 3. Run with NewReno, transfer only 10MB, and enable PCAP capture:
 *      ./ns3 run "picoquic-ns3-example --ccAlgo=newreno --maxBytes=10000000 --enablePcap=1"
 *
 * 4. Run with Cubic but stop the simulation after 50 seconds:
 *      ./ns3 run "picoquic-ns3-example --ccAlgo=cubic --stopTime=50"
 *
 * =====================================================================
 * PLOTTING SCRIPTS (Gnuplot)
 * =====================================================================
 * 
 * To plot the generated `.dat` files, use the following commands inside 
 * the results directory:
 *
 * 1. cwnd.png
 *      gnuplot -e "set terminal pngcairo enhanced color lw 1.5 font 'Times Roman'; set xrange [0:100]; set yrange [0:140]; set output 'cwnd.png'; set xlabel 'Time (sec)'; set ylabel 'Cwnd (Packets)'; set key right top vertical; plot 'cwnd.dat' title 'Cwnd' with lines lw 1.5 lc 'blue'"
 *
 * 2. queueSize.png
 *      gnuplot -e "set terminal pngcairo enhanced color lw 1.5 font 'Times Roman'; set xrange [0:100]; set yrange [0:80]; set output 'queueSize.png'; set xlabel 'Time (sec)'; set ylabel 'Queue occupancy (No. of packets)'; set key right top vertical; plot 'queueSize.dat' title 'Queue Length' with lines lw 1.5 lc 'blue'"
 *
 * 3. throughput.png
 *      gnuplot -e "set terminal pngcairo enhanced color lw 1.5 font 'Times Roman'; set output 'throughput.png'; set xlabel 'Time (s)'; set ylabel 'Throughput (Mbps)'; set xrange [0:100]; set yrange [0:20]; set key right top vertical; plot 'throughput.dat' using 1:2 title 'Throughput' with lines lw 1.5 lc 'blue'"
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/picoquic-client-app.h"
#include "ns3/picoquic-helper.h"
#include "ns3/picoquic-server-app.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <filesystem>
#include <fstream>
#include <sys/stat.h>

using namespace ns3;
using namespace ns3::picoquic_ns3;
using namespace ns3::SystemPath;

std::string dir;
std::ofstream throughput;
std::ofstream queueSize;
std::ofstream cwndFile;

uint64_t prevBytes = 0;
Time prevTime;

Ptr<QuicClientApp> g_clientApp;

void
TraceThroughput(uint64_t* txBytesCounter)
{
    Time curTime = Simulator::Now();
    if (curTime > prevTime)
    {
        double diffUs = (curTime - prevTime).GetMicroSeconds();
        if (diffUs > 0)
        {
            double mbps = 8.0 * (*txBytesCounter - prevBytes) / diffUs;
            throughput << curTime.GetSeconds() << "s " << mbps << " Mbps" << std::endl;
        }
    }
    prevTime = curTime;
    prevBytes = *txBytesCounter;

    Simulator::Schedule(Seconds(0.2), &TraceThroughput, txBytesCounter);
}

void
CheckQueueSize(Ptr<QueueDisc> qd)
{
    uint32_t qsize = qd->GetCurrentSize().GetValue();
    queueSize << Simulator::Now().GetSeconds() << " " << qsize << std::endl;
    Simulator::Schedule(Seconds(0.2), &CheckQueueSize, qd);
}

void
PollCwnd()
{
    if (g_clientApp && g_clientApp->GetConnection() &&
        !g_clientApp->GetConnection()->IsDisconnected())
    {
        double cwndSegments = g_clientApp->GetConnection()->GetCwin() / 1448.0;
        if (cwndSegments > 0)
        {
            cwndFile << Simulator::Now().GetSeconds() << " " << cwndSegments << std::endl;
        }
    }
    Simulator::Schedule(MilliSeconds(100), &PollCwnd);
}

int
main(int argc, char* argv[])
{
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%d-%m-%Y-%I-%M-%S", timeinfo);
    std::string currentTime(buffer);

    std::string ccAlgo = "cubic";
    std::string queueDisc = "FifoQueueDisc";
    uint64_t maxBytes = 0;
    bool bql = true;
    bool enablePcap = true; // Enable/Disable pcap file generation
    bool enableQlog = true; // Enable/Disable QLOG file generation
    Time stopTime = Seconds(100);

    CommandLine cmd(__FILE__);
    cmd.AddValue("ccAlgo",
                 "Congestion control to use in Picoquic: cubic, bbr, newreno, etc",
                 ccAlgo);
    cmd.AddValue("enableQlog", "Enable/Disable QLOG file generation", enableQlog);
    cmd.AddValue("maxBytes", "Total bytes to transfer (0 for unlimited)", maxBytes);
    cmd.AddValue("enablePcap", "Enable/Disable pcap file generation", enablePcap);
    cmd.AddValue("stopTime", "Stop time for applications", stopTime);
    cmd.Parse(argc, argv);

    queueDisc = std::string("ns3::") + queueDisc;

    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue(QueueSize("1p")));
    Config::SetDefault(queueDisc + "::MaxSize", QueueSizeValue(QueueSize("100p")));

    NodeContainer sender, receiver, routers;
    sender.Create(1);
    receiver.Create(1);
    routers.Create(2);

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottleneckLink.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper edgeLink;
    edgeLink.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    edgeLink.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer senderEdge = edgeLink.Install(sender.Get(0), routers.Get(0));
    NetDeviceContainer r1r2 = bottleneckLink.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer receiverEdge = edgeLink.Install(routers.Get(1), receiver.Get(0));

    InternetStackHelper internet;
    internet.Install(sender);
    internet.Install(receiver);
    internet.Install(routers);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc(queueDisc);
    if (bql)
    {
        tch.SetQueueLimits("ns3::DynamicQueueLimits", "HoldTime", StringValue("1000ms"));
    }
    tch.Install(senderEdge);
    tch.Install(receiverEdge);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(r1r2);

    ipv4.NewNetwork();
    Ipv4InterfaceContainer is1 = ipv4.Assign(senderEdge);

    ipv4.NewNetwork();
    Ipv4InterfaceContainer ir1 = ipv4.Assign(receiverEdge);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint64_t serverBytesRx = 0;
    uint64_t streamsOpened = 0;
    bool clientConnected = false;
    Time connectionTime;
    Time completionTime;
    Time closedTime;

    dir = "picoquic-results/" + ccAlgo + "-" + currentTime + "/";
    MakeDirectories(dir);

    std::string qlogDir = "";
    if (enableQlog)
    {
        qlogDir = dir + "qlog/";
        MakeDirectories(qlogDir);
    }

    uint16_t port = 4433;
    InetSocketAddress serverAddr(ir1.GetAddress(1), port);
    QuicServerHelper server(serverAddr);
    server.SetAttribute("CongestionControl", StringValue(ccAlgo));
    if (!qlogDir.empty())
    {
        server.SetAttribute("QlogDir", StringValue(qlogDir));
    }

#ifdef PICOQUIC_NS3_PICOQUIC_PREFIX
    server.SetAttribute("CertFile",
                        StringValue(std::string(PICOQUIC_NS3_PICOQUIC_PREFIX) + "/certs/cert.pem"));
    server.SetAttribute("KeyFile",
                        StringValue(std::string(PICOQUIC_NS3_PICOQUIC_PREFIX) + "/certs/key.pem"));
#endif

    ApplicationContainer serverApps = server.Install(receiver.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(stopTime);

    Ptr<QuicServerApp> serverApp = DynamicCast<QuicServerApp>(serverApps.Get(0));
    serverApp->m_rxTrace.ConnectWithoutContext(MakeBoundCallback(
        +[](uint64_t* counter, Ptr<const QuicServerApp>, uint64_t bytes) { *counter += bytes; },
        &serverBytesRx));
    serverApp->m_connectionAccepted.ConnectWithoutContext(MakeBoundCallback(
        +[](uint64_t* counter, Ptr<const QuicServerApp>) { (*counter)++; },
        &streamsOpened));

    QuicClientHelper client(serverAddr);
    client.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    client.SetAttribute("CongestionControl", StringValue(ccAlgo));
    if (!qlogDir.empty())
    {
        client.SetAttribute("QlogDir", StringValue(qlogDir));
    }

#ifdef PICOQUIC_NS3_PICOQUIC_PREFIX
    client.SetAttribute(
        "CertRootFile",
        StringValue(std::string(PICOQUIC_NS3_PICOQUIC_PREFIX) + "/certs/test-ca.crt"));
#endif

    ApplicationContainer clientApps = client.Install(sender.Get(0));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(stopTime);

    g_clientApp = DynamicCast<QuicClientApp>(clientApps.Get(0));

    static uint64_t g_txBytes = 0;
    g_clientApp->m_txTrace.ConnectWithoutContext(MakeBoundCallback(
        +[](uint64_t* counter, Ptr<const QuicClientApp>, uint64_t bytes) { *counter += bytes; },
        &g_txBytes));

    g_clientApp->m_connectionEstablished.ConnectWithoutContext(MakeBoundCallback(
        +[](bool* connected, Time* connTime, Ptr<const QuicClientApp>) {
            *connected = true;
            *connTime = Simulator::Now();
        },
        &clientConnected,
        &connectionTime));

    g_clientApp->m_connectionClosed.ConnectWithoutContext(MakeBoundCallback(
        +[](Time* done, Ptr<const QuicClientApp>) { *done = Simulator::Now(); },
        &closedTime));

    g_clientApp->m_transferComplete.ConnectWithoutContext(MakeBoundCallback(
        +[](Time* done, Ptr<const QuicClientApp>) { *done = Simulator::Now(); },
        &completionTime));

    tch.Uninstall(routers.Get(0)->GetDevice(1));
    QueueDiscContainer qd = tch.Install(routers.Get(0)->GetDevice(1));
    Simulator::ScheduleNow(&CheckQueueSize, qd.Get(0));

    if (enablePcap)
    {
        MakeDirectories(dir + "pcap/");
        bottleneckLink.EnablePcapAll(dir + "pcap/" + ccAlgo, true);
    }

    throughput.open(dir + "throughput.dat", std::ios::out);
    queueSize.open(dir + "queueSize.dat", std::ios::out);
    cwndFile.open(dir + "cwnd.dat", std::ios::out);

    Simulator::Schedule(Seconds(0.1) + MilliSeconds(1), &TraceThroughput, &g_txBytes);
    Simulator::Schedule(Seconds(0.1) + MilliSeconds(1), &PollCwnd);

    Simulator::Stop(stopTime + TimeStep(1));
    Simulator::Run();
    Simulator::Destroy();

    throughput.close();
    queueSize.close();
    cwndFile.close();

    double transferDurationSec = 0.0;
    if (completionTime.GetSeconds() > 0.0)
    {
        transferDurationSec = (completionTime - connectionTime).GetSeconds();
    }
    else if (clientConnected)
    {
        transferDurationSec = (stopTime - connectionTime).GetSeconds();
    }

    double uploadMbps = (transferDurationSec > 0.0)
                            ? (static_cast<double>(g_txBytes) * 8.0 / transferDurationSec / 1e6)
                            : 0.0;
    double downloadMbps =
        (transferDurationSec > 0.0)
            ? (static_cast<double>(serverBytesRx) * 8.0 / transferDurationSec / 1e6)
            : 0.0;

    std::cout << "\n"
              << "======= picoquic-ns3 example results ========\n"
              << "  Topology      : " << "1000Mbps-10Mbps-1000Mbps\n"
              << "  CC algorithm  : " << ccAlgo << "\n"
              << "  Max Bytes     : " << (maxBytes == 0 ? "Unlimited" : std::to_string(maxBytes))
              << "\n"
              << "  Streams opened: " << streamsOpened << "\n"
              << "  Upload bytes  : " << g_txBytes << "\n"
              << "  Server rx     : " << serverBytesRx << "\n"
              << "  Transfer time : " << transferDurationSec << " s\n"
              << "  Upload Mbps   : " << uploadMbps << " Mbps\n"
              << "  Server rx Mbps: " << downloadMbps << " Mbps\n"
              << "=============================================\n\n";

    return 0;
}
