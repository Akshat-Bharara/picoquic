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

#include "picoquic-client-app.h"

#include "picoquic-sim-time.h"

#include "ns3/boolean.h"
#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cstring>
#include <picoquic.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PicoquicClientApp");

namespace picoquic_ns3
{

NS_OBJECT_ENSURE_REGISTERED(QuicClientApp);

TypeId
QuicClientApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::picoquic_ns3::QuicClientApp")
            .SetParent<Application>()
            .SetGroupName("PicoquicNs3")
            .AddConstructor<QuicClientApp>()
            .AddAttribute("RemoteAddress",
                          "The destination address of the server.",
                          AddressValue(InetSocketAddress(Ipv4Address::GetLoopback(), 4433)),
                          MakeAddressAccessor(&QuicClientApp::m_peerAddr),
                          MakeAddressChecker())
            .AddAttribute("MaxBytes",
                          "Total bytes to send. 0 means unlimited.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&QuicClientApp::m_maxBytes),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("Alpn",
                          "ALPN token to negotiate.",
                          StringValue("h3"),
                          MakeStringAccessor(&QuicClientApp::m_alpn),
                          MakeStringChecker())
            .AddAttribute("Sni",
                          "Server Name Indication (must match server certificate CN).",
                          StringValue("test.example.com"),
                          MakeStringAccessor(&QuicClientApp::m_sni),
                          MakeStringChecker())
            .AddAttribute("CongestionControl",
                          "Congestion-control algorithm (e.g. bbr, cubic, newreno).",
                          StringValue(""),
                          MakeStringAccessor(&QuicClientApp::m_ccAlgo),
                          MakeStringChecker())
            .AddAttribute("QlogDir",
                          "Directory for QLOG output files (empty string = disabled).",
                          StringValue(""),
                          MakeStringAccessor(&QuicClientApp::m_qlogDir),
                          MakeStringChecker())
            .AddAttribute("DisableCertVerification",
                          "Disable TLS certificate verification.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&QuicClientApp::m_disableCertVerify),
                          MakeBooleanChecker())
            .AddAttribute("CertRootFile",
                          "Path to trusted root CA file for client verification.",
                          StringValue(""),
                          MakeStringAccessor(&QuicClientApp::m_certRootFile),
                          MakeStringChecker())
            .AddTraceSource("ConnectionEstablished",
                            "Fired when the QUIC connection is ready.",
                            MakeTraceSourceAccessor(&QuicClientApp::m_connectionEstablished),
                            "ns3::picoquic_ns3::QuicClientApp::TracedCallback")
            .AddTraceSource("ConnectionClosed",
                            "Fired when the QUIC connection closes.",
                            MakeTraceSourceAccessor(&QuicClientApp::m_connectionClosed),
                            "ns3::picoquic_ns3::QuicClientApp::TracedCallback")
            .AddTraceSource("Tx",
                            "Trace of bytes submitted to the stream.",
                            MakeTraceSourceAccessor(&QuicClientApp::m_txTrace),
                            "ns3::picoquic_ns3::QuicClientApp::TxTracedCallback")
            .AddTraceSource("TransferComplete",
                            "Fired when the last byte has been submitted to the stream.",
                            MakeTraceSourceAccessor(&QuicClientApp::m_transferComplete),
                            "ns3::picoquic_ns3::QuicClientApp::TracedCallback");
    return tid;
}

QuicClientApp::QuicClientApp()
    : m_peerAddr(InetSocketAddress(Ipv4Address::GetLoopback(), 4433)),
      m_alpn("h3"),
      m_sni("test.example.com"),
      m_maxBytes(0),
      m_disableCertVerify(false),
      m_conn(nullptr),
      m_streamId(0),
      m_totalSent(0),
      m_connected(false)
{
    NS_LOG_FUNCTION(this);
}

QuicClientApp::~QuicClientApp()
{
    NS_LOG_FUNCTION(this);
}

void
QuicClientApp::SetMaxBytes(uint64_t maxBytes)
{
    m_maxBytes = maxBytes;
}

void
QuicClientApp::SetRemote(const InetSocketAddress& addr)
{
    m_peerAddr = Address(addr);
}

void
QuicClientApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_transport.reset();
    Application::DoDispose();
}

/* ================================================================== */
/*  Application lifecycle                                             */
/* ================================================================== */

void
QuicClientApp::StartApplication()
{
    NS_LOG_FUNCTION(this);

    QuicContextConfig cfg;
    cfg.defaultAlpn = m_alpn.c_str();
    cfg.disableCertVerification = m_disableCertVerify;
    cfg.certRootFile = m_certRootFile.empty() ? nullptr : m_certRootFile.c_str();
    if (!m_ccAlgo.empty())
    {
        cfg.ccAlgorithm = m_ccAlgo;
    }

    QuicContext ctx(cfg);

    InetSocketAddress local(Ipv4Address::GetAny(), 0);

    m_transport = std::make_unique<QuicTransport>(std::move(ctx), GetNode(), local);
    m_transport->Start();

    if (!m_qlogDir.empty())
    {
        m_transport->GetContext().EnableQlog(m_qlogDir);
        NS_LOG_INFO("QuicClientApp QLOG enabled, dir=" << m_qlogDir);
    }

    InetSocketAddress remote = InetSocketAddress::ConvertFrom(m_peerAddr);

    m_conn = m_transport->CreateClientConnection(
        remote,
        m_sni,
        m_alpn,
        [this](picoquic_cnx_t* cnx,
               uint64_t streamId,
               uint8_t* bytes,
               size_t length,
               picoquic_call_back_event_t event,
               void* streamCtx) -> int {
            return AppCallback(cnx, streamId, bytes, length, event, streamCtx);
        });

    NS_LOG_INFO("QuicClientApp started, connecting to " << remote);
}

void
QuicClientApp::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_conn && !m_conn->IsDisconnected())
    {
        m_conn->Close(0);
    }

    if (m_transport)
    {
        m_transport->Stop();
    }

    m_connected = false;
    NS_LOG_INFO("QuicClientApp stopped (totalSent=" << m_totalSent << ")");
}

/* ================================================================== */
/*  Picoquic application callback                                     */
/* ================================================================== */

int
QuicClientApp::AppCallback(picoquic_cnx_t* cnx,
                           uint64_t streamId,
                           uint8_t* bytes,
                           size_t length,
                           picoquic_call_back_event_t event,
                           void* streamCtx)
{
    NS_LOG_FUNCTION(this << event);

    switch (event)
    {
    case picoquic_callback_ready:
    case picoquic_callback_almost_ready: {
        NS_LOG_INFO("Client connection ready");
        m_connected = true;
        m_connectionEstablished(this);

        m_streamId = 0;

        if (m_maxBytes == 0 || m_totalSent < m_maxBytes)
        {
            picoquic_mark_active_stream(cnx, m_streamId, 1, nullptr);
        }
        break;
    }

    case picoquic_callback_prepare_to_send: {
        
        if (!m_connected)
        {
            return 0;
        }

        size_t available = length;
        bool isFin = false;

        if (m_maxBytes > 0)
        {
            uint64_t remaining = m_maxBytes - m_totalSent;
            if (remaining == 0)
            {
                return 0;
            }
            available = std::min(available, static_cast<size_t>(remaining));
            isFin = (m_totalSent + available >= m_maxBytes);
        }

        uint8_t* buf = picoquic_provide_stream_data_buffer(
            bytes,
            available,
            isFin ? 1 : 0,
            (isFin || (m_maxBytes > 0 && m_totalSent + available >= m_maxBytes)) ? 0 : 1);

        if (buf != nullptr)
        {
            std::memset(buf, 0x42, available);
            m_totalSent += available;
            m_txTrace(this, available);
            if (isFin)
            {
                m_transferComplete(this);
                NS_LOG_INFO("All " << m_totalSent << " bytes submitted to stream (FIN sent)");
            }
        }
        break;
    }

    case picoquic_callback_stream_data:
    case picoquic_callback_stream_fin: {
        
        NS_LOG_LOGIC("Client received " << length << " bytes on stream " << streamId);
        break;
    }

    case picoquic_callback_close:
    case picoquic_callback_application_close:
    case picoquic_callback_stateless_reset: {
        NS_LOG_INFO("Client connection closed (event=" << event << ")");
        m_connected = false;
        m_connectionClosed(this);
        break;
    }

    case picoquic_callback_stream_reset:
    case picoquic_callback_stop_sending: {
        NS_LOG_WARN("Stream " << streamId << " reset/stop_sending");
        picoquic_reset_stream(cnx, streamId, 0);
        break;
    }

    default:
        break;
    }

    return 0;
}

} 
} 
