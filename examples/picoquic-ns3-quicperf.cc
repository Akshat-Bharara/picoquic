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
 * Example: QUIC performance testing using the quicperf protocol over
 * a simulated point-to-point link.
 *
 * Topology:
 *
 *   n0 (client, 10.1.1.1) ---- P2P (10 Mbps, 20 ms) ---- n1 (server, 10.1.1.2:4433)
 *
 * The example uses the quicperf protocol defined in
 *   https://datatracker.ietf.org/doc/draft-banks-quic-performance/
 * and implemented in picoquic's picohttp/quicperf.{h,c}.
 *
 * =====================================================================
 * COMMAND-LINE PARAMETERS
 * =====================================================================
 *
 *   --scenario   quicperf scenario string  (default: "=b1:*1:397:5000000;")
 *   --ccAlgo     Congestion control: bbr | cubic | newreno  (default: bbr)
 *   --simTime    Simulation duration in seconds  (default: 30.0)
 *   --dataRate   P2P link data rate  (default: 10Mbps)
 *   --delay      P2P one-way propagation delay  (default: 20ms)
 *   --pcap       Enable PCAP capture on all devices  (default: false)
 *   --csvFile    Path for per-frame CSV output; only populated for media
 *                ('s') and datagram ('d') stream types  (default: "")
 *
 * =====================================================================
 * SCENARIO STRING SYNTAX
 * =====================================================================
 *
 * Each semicolon-terminated token describes one stream (or group):
 *
 *   [=id:] [=prev_id:] [*repeat:] [media_type] ...
 *
 *   media_type is one of:
 *     (omitted)  batch stream  — bulk upload + download, summary stats only
 *     s          media stream  — framed delivery; per-frame CSV rows written
 *     d          datagram      — unreliable; per-datagram CSV rows written
 *
 *   Batch stream fields (after optional prefixes):
 *     post_size : response_size
 *
 *   Media stream fields (after 's'):
 *     freq : [p<priority>:] [C|S:] [n<nb_frames>:] [frame_size:] [G<group>:] [I<first>:]
 *
 * =====================================================================
 * USAGE EXAMPLES
 * =====================================================================
 *
 * 1. Run with all defaults (batch, 5 MB download, BBR, 10 Mbps / 20 ms):
 *
 *      ./ns3 run picoquic-ns3-quicperf
 *
 * 2. Change the congestion control algorithm:
 *
 *      ./ns3 run "picoquic-ns3-quicperf --ccAlgo=cubic"
 *      ./ns3 run "picoquic-ns3-quicperf --ccAlgo=newreno"
 *
 * 3. Custom batch scenario — post 1 kB, receive 10 MB:
 *
 *      ./ns3 run "picoquic-ns3-quicperf --scenario==b1:*1:1000:10000000;"
 *
 * 4. Change link parameters (100 Mbps, 5 ms delay):
 *
 *      ./ns3 run "picoquic-ns3-quicperf --dataRate=100Mbps --delay=5ms"
 *
 * 5. Media stream — 30 fps, 100 frames, 1400 B/frame, with CSV output:
 *
 *      ./ns3 run "picoquic-ns3-quicperf --scenario==s1:*1:s30:n100:1400; \
 *                 --csvFile=quicperf-media.csv"
 *
 *    CSV columns: stream, repeat, group, frame, init_time, recv_time
 *
 * 6. Enable PCAP capture:
 *
 *      ./ns3 run "picoquic-ns3-quicperf --pcap=true"
 *
 *    Produces: picoquic-ns3-quicperf-0-0.pcap  (client)
 *              picoquic-ns3-quicperf-1-0.pcap  (server)
 *
 * 7. Combine multiple options:
 *
 *      ./ns3 run "picoquic-ns3-quicperf --scenario==b1:*1:1000:50000000; \
 *                 --ccAlgo=cubic --dataRate=50Mbps --delay=10ms \
 *                 --simTime=30 --pcap=true"
 */

#include "picoquic.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/picoquic-context.h"
#include "ns3/picoquic-transport.h"
#include "ns3/point-to-point-module.h"

#include "quicperf.h"

#include <cstdio>
#include <cstring>

using namespace ns3;
using namespace ns3::picoquic_ns3;

NS_LOG_COMPONENT_DEFINE("PicoquicQuicperfExample");

class QuicperfServerApp : public Application
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("QuicperfServerApp").SetParent<Application>();
        return tid;
    }

    QuicperfServerApp(InetSocketAddress listenAddr,
                      const std::string& certFile,
                      const std::string& keyFile,
                      const std::string& ccAlgo)
        : m_listenAddr(listenAddr),
          m_certFile(certFile),
          m_keyFile(keyFile),
          m_ccAlgo(ccAlgo),
          m_bytesRx(0)
    {
    }

    uint64_t GetBytesRx() const
    {
        return m_bytesRx;
    }

  private:
    void StartApplication() override
    {
        QuicContextConfig cfg;
        cfg.certFile = m_certFile.empty() ? nullptr : m_certFile.c_str();
        cfg.keyFile = m_keyFile.empty() ? nullptr : m_keyFile.c_str();
        cfg.defaultAlpn = QUICPERF_ALPN;
        cfg.ccAlgorithm = m_ccAlgo;
        cfg.disableCertVerification = true; /* server-side: no client cert needed */

        m_transport = std::make_unique<QuicTransport>(QuicContext(cfg), GetNode(), m_listenAddr);

        picoquic_set_default_callback(m_transport->GetQuicNative(), quicperf_callback, nullptr);

        m_transport->Start();
        NS_LOG_INFO("QuicperfServer started on " << m_listenAddr);
    }

    void StopApplication() override
    {
        if (m_transport)
        {
            m_transport->Stop();
        }
    }

    InetSocketAddress m_listenAddr;
    std::string m_certFile;
    std::string m_keyFile;
    std::string m_ccAlgo;
    uint64_t m_bytesRx;

    std::unique_ptr<QuicTransport> m_transport;
};

class QuicperfClientApp : public Application
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("QuicperfClientApp").SetParent<Application>();
        return tid;
    }

    QuicperfClientApp(InetSocketAddress serverAddr,
                      const std::string& scenario,
                      const std::string& certRootFile,
                      const std::string& ccAlgo)
        : m_serverAddr(serverAddr),
          m_scenario(scenario),
          m_certRootFile(certRootFile),
          m_ccAlgo(ccAlgo),
          m_perfCtx(nullptr),
          m_transferEndTime(Seconds(0))
    {
    }

    ~QuicperfClientApp() override
    {
        if (m_perfCtx)
        {
            quicperf_delete_ctx(m_perfCtx);
        }
        if (m_csvFp)
        {
            std::fclose(m_csvFp);
        }
    }

    quicperf_ctx_t* GetPerfCtx() const
    {
        return m_perfCtx;
    }

    Time GetTransferEndTime() const
    {
        return m_transferEndTime;
    }

    /**
     * Optionally set a CSV report file path before the simulation starts.
     * quicperf will write one row per media frame or datagram received:
     *   stream, repeat, group, frame, init_time, recv_time
     */
    void SetCsvReportFile(const std::string& path)
    {
        m_csvFilePath = path;
    }

  private:
    void StartApplication() override
    {
        m_perfCtx = quicperf_create_ctx(m_scenario.c_str(), stderr);
        if (!m_perfCtx)
        {
            NS_FATAL_ERROR("quicperf_create_ctx failed — invalid scenario string: " << m_scenario);
            return;
        }

        if (!m_csvFilePath.empty())
        {
            m_csvFp = std::fopen(m_csvFilePath.c_str(), "w");
            if (m_csvFp)
            {
                m_perfCtx->report_file = m_csvFp;
                NS_LOG_INFO("CSV report will be written to " << m_csvFilePath);
            }
            else
            {
                NS_LOG_WARN("Could not open CSV report file: " << m_csvFilePath);
            }
        }

        QuicContextConfig cfg;
        cfg.certRootFile = m_certRootFile.empty() ? nullptr : m_certRootFile.c_str();
        cfg.defaultAlpn = QUICPERF_ALPN;
        cfg.ccAlgorithm = m_ccAlgo;
        cfg.disableCertVerification = m_certRootFile.empty();

        InetSocketAddress clientBind(Ipv4Address::GetAny(), 0);
        m_transport = std::make_unique<QuicTransport>(QuicContext(cfg), GetNode(), clientBind);
        m_transport->Start();

        quicperf_ctx_t* ctx = m_perfCtx;       
        Time* endTimePtr = &m_transferEndTime; 
        m_transport->CreateClientConnection(
            m_serverAddr,
            "test.example.com",
            QUICPERF_ALPN,
            [ctx, endTimePtr](picoquic_cnx_t* cnx,
                              uint64_t streamId,
                              uint8_t* bytes,
                              size_t length,
                              picoquic_call_back_event_t event,
                              void* streamCtx) -> int {
                if (event == picoquic_callback_stream_fin)
                {
                    *endTimePtr = Simulator::Now();
                }
                return quicperf_callback(cnx, streamId, bytes, length, event, ctx, streamCtx);
            });

        NS_LOG_INFO("QuicperfClient connecting to " << m_serverAddr
                                                    << "  scenario: " << m_scenario);
    }

    void StopApplication() override
    {
        if (m_transport)
        {
            m_transport->Stop();
        }
    }

    InetSocketAddress m_serverAddr;
    std::string m_scenario;
    std::string m_certRootFile;
    std::string m_ccAlgo;
    std::string m_csvFilePath;
    quicperf_ctx_t* m_perfCtx;
    FILE* m_csvFp{nullptr};
    Time m_transferEndTime;

    std::unique_ptr<QuicTransport> m_transport;
};

int
main(int argc, char* argv[])
{
    /* ------------------------------------------------------------ */
    /*  Command-line configuration                                  */
    /* ------------------------------------------------------------ */
    /* Default: one batch stream id "b1", repeat once, client posts 397 bytes,
     * server sends 5 MB back. */
    std::string scenario = "=b1:*1:397:5000000;";
    std::string ccAlgo = "bbr";
    double simTimeSec = 30.0;
    std::string dataRate = "10Mbps";
    std::string delay = "20ms";
    bool pcapEnabled = false;
    std::string csvFile; /* empty = no CSV output */

    CommandLine cmd(__FILE__);
    cmd.AddValue("scenario", "quicperf scenario string (e.g. \"=b1:*1:397:5000000;\")", scenario);
    cmd.AddValue("ccAlgo", "Congestion control algorithm (bbr, cubic, newreno)", ccAlgo);
    cmd.AddValue("simTime", "Simulation duration in seconds", simTimeSec);
    cmd.AddValue("dataRate", "P2P link data rate (e.g. 10Mbps)", dataRate);
    cmd.AddValue("delay", "P2P one-way propagation delay (e.g. 20ms)", delay);
    cmd.AddValue("pcap", "Enable PCAP capture on all devices", pcapEnabled);
    cmd.AddValue("csvFile",
                 "Path for per-frame CSV output (media/'s' and datagram/'d' streams only)",
                 csvFile);
    cmd.Parse(argc, argv);

    /* ------------------------------------------------------------ */
    /*  Topology                                                    */
    /* ------------------------------------------------------------ */
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));
    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    if (pcapEnabled)
    {
        p2p.EnablePcapAll("picoquic-ns3-quicperf");
    }

    /* ------------------------------------------------------------ */
    /*  Certificate paths (from compile-time macro)                 */
    /* ------------------------------------------------------------ */
    std::string certFile;
    std::string keyFile;
    std::string certRootFile;

#ifdef PICOQUIC_NS3_PICOQUIC_PREFIX
    const std::string prefix = PICOQUIC_NS3_PICOQUIC_PREFIX;
    certFile = prefix + "/certs/cert.pem";
    keyFile = prefix + "/certs/key.pem";
    certRootFile = prefix + "/certs/test-ca.crt";
#endif

    /* ------------------------------------------------------------ */
    /*  Server on n1 (10.1.1.2 : 4433)                             */
    /* ------------------------------------------------------------ */
    InetSocketAddress serverAddr(interfaces.GetAddress(1), 4433);

    Ptr<QuicperfServerApp> serverApp =
        CreateObject<QuicperfServerApp>(serverAddr, certFile, keyFile, ccAlgo);
    nodes.Get(1)->AddApplication(serverApp);
    serverApp->SetStartTime(Seconds(0.0));
    serverApp->SetStopTime(Seconds(simTimeSec));

    /* ------------------------------------------------------------ */
    /*  Client on n0                                                */
    /* ------------------------------------------------------------ */
    Ptr<QuicperfClientApp> clientApp =
        CreateObject<QuicperfClientApp>(serverAddr, scenario, certRootFile, ccAlgo);
    if (!csvFile.empty())
    {
        clientApp->SetCsvReportFile(csvFile);
    }
    nodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(1.0));
    clientApp->SetStopTime(Seconds(simTimeSec));

    /* ------------------------------------------------------------ */
    /*  Run                                                         */
    /* ------------------------------------------------------------ */
    Simulator::Stop(Seconds(simTimeSec));
    Simulator::Run();
    Simulator::Destroy();

    /* ------------------------------------------------------------ */
    /*  Print results                                               */
    /* ------------------------------------------------------------ */
    quicperf_ctx_t* ctx = clientApp->GetPerfCtx();

    std::cout << "\n"
              << "======= picoquic-ns3 quicperf results =======\n"
              << "  Topology      : " << dataRate << ", " << delay << " one-way delay\n"
              << "  CC algorithm  : " << ccAlgo << "\n"
              << "  Scenario      : " << scenario << "\n";
    if (!csvFile.empty())
    {
        std::cout << "  CSV report    : " << csvFile << "\n";
    }

    if (ctx)
    {
        Time transferEnd = clientApp->GetTransferEndTime();
        double durationSec = 0.0;
        if (transferEnd > Seconds(1.0))
        {
            durationSec = (transferEnd - Seconds(1.0)).GetSeconds();
        }

        double uploadMbps = (durationSec > 0)
                                ? (static_cast<double>(ctx->data_sent) * 8.0 / durationSec / 1e6)
                                : 0.0;
        double downloadMbps =
            (durationSec > 0) ? (static_cast<double>(ctx->data_received) * 8.0 / durationSec / 1e6)
                              : 0.0;

        std::cout << "  Streams opened: " << ctx->nb_streams << "\n"
                  << "  Upload bytes  : " << ctx->data_sent << "\n"
                  << "  Download bytes: " << ctx->data_received << "\n"
                  << "  Transfer time : " << durationSec << " s\n"
                  << "  Upload Mbps   : " << uploadMbps << "\n"
                  << "  Download Mbps : " << downloadMbps << "\n";

        quicperf_print_report(stdout, ctx);
    }
    else
    {
        std::cout << "  (no quicperf context — client did not start)\n";
    }

    std::cout << "=============================================\n\n";

    return 0;
}
