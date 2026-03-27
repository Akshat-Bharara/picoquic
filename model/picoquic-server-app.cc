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

#include "picoquic-server-app.h"

#include "picoquic-sim-time.h"

#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include <cstring>
#include <picoquic.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PicoquicServerApp");

namespace picoquic_ns3
{

NS_OBJECT_ENSURE_REGISTERED(QuicServerApp);

TypeId
QuicServerApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::picoquic_ns3::QuicServerApp")
            .SetParent<Application>()
            .SetGroupName("PicoquicNs3")
            .AddConstructor<QuicServerApp>()
            .AddAttribute("ListenAddress",
                          "Local address and port to listen on.",
                          AddressValue(InetSocketAddress(Ipv4Address::GetAny(), 4433)),
                          MakeAddressAccessor(&QuicServerApp::m_listenAddr),
                          MakeAddressChecker())
            .AddAttribute("CertFile",
                          "Path to the PEM certificate file.",
                          StringValue(""),
                          MakeStringAccessor(&QuicServerApp::m_certFile),
                          MakeStringChecker())
            .AddAttribute("KeyFile",
                          "Path to the PEM private-key file.",
                          StringValue(""),
                          MakeStringAccessor(&QuicServerApp::m_keyFile),
                          MakeStringChecker())
            .AddAttribute("Alpn",
                          "ALPN token to negotiate.",
                          StringValue("h3"),
                          MakeStringAccessor(&QuicServerApp::m_alpn),
                          MakeStringChecker())
            .AddAttribute("CongestionControl",
                          "Congestion-control algorithm.",
                          StringValue(""),
                          MakeStringAccessor(&QuicServerApp::m_ccAlgo),
                          MakeStringChecker())
            .AddAttribute("QlogDir",
                          "Directory for QLOG output files (empty string = disabled).",
                          StringValue(""),
                          MakeStringAccessor(&QuicServerApp::m_qlogDir),
                          MakeStringChecker())
            .AddAttribute("EchoMode",
                          "If true, echo received stream data back.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&QuicServerApp::m_echoMode),
                          MakeBooleanChecker())
            .AddTraceSource("ConnectionAccepted",
                            "Fired when a new QUIC connection is accepted.",
                            MakeTraceSourceAccessor(&QuicServerApp::m_connectionAccepted),
                            "ns3::picoquic_ns3::QuicServerApp::TracedCallback")
            .AddTraceSource("ConnectionClosed",
                            "Fired when a connection closes.",
                            MakeTraceSourceAccessor(&QuicServerApp::m_connectionClosed),
                            "ns3::picoquic_ns3::QuicServerApp::TracedCallback")
            .AddTraceSource("Rx",
                            "Trace of bytes received on streams.",
                            MakeTraceSourceAccessor(&QuicServerApp::m_rxTrace),
                            "ns3::picoquic_ns3::QuicServerApp::RxTracedCallback");
    return tid;
}

QuicServerApp::QuicServerApp()
    : m_listenAddr(InetSocketAddress(Ipv4Address::GetAny(), 4433)),
      m_alpn("h3"),
      m_echoMode(false)
{
    NS_LOG_FUNCTION(this);
}

QuicServerApp::~QuicServerApp()
{
    NS_LOG_FUNCTION(this);
}

void
QuicServerApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_transport.reset();
    Application::DoDispose();
}

/* ================================================================== */
/*  Application lifecycle                                             */
/* ================================================================== */

void
QuicServerApp::StartApplication()
{
    NS_LOG_FUNCTION(this);

    QuicContextConfig cfg;
    cfg.certFile = m_certFile.empty() ? nullptr : m_certFile.c_str();
    cfg.keyFile = m_keyFile.empty() ? nullptr : m_keyFile.c_str();
    cfg.defaultAlpn = m_alpn.c_str();
    if (!m_ccAlgo.empty())
    {
        cfg.ccAlgorithm = m_ccAlgo;
    }
    
    cfg.disableCertVerification = true;

    QuicContext ctx(cfg);

    
    InetSocketAddress listenIsa = InetSocketAddress::ConvertFrom(m_listenAddr);

    m_transport = std::make_unique<QuicTransport>(std::move(ctx), GetNode(), listenIsa);

   
    picoquic_set_default_callback(m_transport->GetQuicNative(), &QuicServerApp::AppCallback, this);

    m_transport->SetNewConnectionCallback([this](picoquic_cnx_t* cnx) { OnNewConnection(cnx); });

    m_transport->Start();

    if (!m_qlogDir.empty())
    {
        m_transport->GetContext().EnableQlog(m_qlogDir);
        NS_LOG_INFO("QuicServerApp QLOG enabled, dir=" << m_qlogDir);
    }

    NS_LOG_INFO("QuicServerApp listening on " << listenIsa);
}

void
QuicServerApp::StopApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_transport)
    {
        m_transport->Stop();
    }
    NS_LOG_INFO("QuicServerApp stopped");
}

/* ================================================================== */
/*  New connection handler                                            */
/* ================================================================== */

void
QuicServerApp::OnNewConnection(picoquic_cnx_t* cnx)
{
    NS_LOG_FUNCTION(this << cnx);

    picoquic_set_callback(cnx, &QuicServerApp::AppCallback, this);

    m_connectionAccepted(this);
    NS_LOG_INFO("Server accepted new QUIC connection");
}

/* ================================================================== */
/*  Picoquic application callback (static, C-compatible signature)    */
/* ================================================================== */

int
QuicServerApp::AppCallback(picoquic_cnx_t* cnx,
                           uint64_t streamId,
                           uint8_t* bytes,
                           size_t length,
                           picoquic_call_back_event_t event,
                           void* callbackCtx,
                           void* streamCtx)
{
    auto* self = static_cast<QuicServerApp*>(callbackCtx);

    switch (event)
    {
    case picoquic_callback_ready:
    case picoquic_callback_almost_ready:
        NS_LOG_INFO("Server connection ready");
        break;

    case picoquic_callback_stream_data:
    case picoquic_callback_stream_fin: {
        NS_LOG_LOGIC("Server received " << length << " bytes on stream " << streamId
                                        << (event == picoquic_callback_stream_fin ? " [FIN]" : ""));

        auto* sctx = static_cast<ServerStreamCtx*>(streamCtx);
        if (sctx == nullptr)
        {
            sctx = new ServerStreamCtx();
            sctx->streamId = streamId;
            picoquic_set_app_stream_ctx(cnx, streamId, sctx);
        }

        sctx->bytesReceived += length;

        if (self)
        {
            self->m_rxTrace(self, length);
        }

        if (self && self->m_echoMode && length > 0)
        {
            bool isFin = (event == picoquic_callback_stream_fin);
            picoquic_add_to_stream(cnx, streamId, bytes, length, isFin ? 1 : 0);
        }
        else if (event == picoquic_callback_stream_fin)
        {
            picoquic_add_to_stream(cnx, streamId, nullptr, 0, 1);
        }

        if (event == picoquic_callback_stream_fin)
        {
            sctx->finReceived = true;
        }
        break;
    }

    case picoquic_callback_stream_reset:
    case picoquic_callback_stop_sending: {
        NS_LOG_WARN("Server stream " << streamId << " reset/stop_sending");
        auto* sctx = static_cast<ServerStreamCtx*>(streamCtx);
        if (sctx)
        {
            delete sctx;
            picoquic_set_app_stream_ctx(cnx, streamId, nullptr);
        }
        picoquic_reset_stream(cnx, streamId, 0);
        break;
    }

    case picoquic_callback_close:
    case picoquic_callback_application_close:
    case picoquic_callback_stateless_reset: {
        NS_LOG_INFO("Server connection closed (event=" << event << ")");
        if (self)
        {
            self->m_connectionClosed(self);
        }
        break;
    }

    case picoquic_callback_prepare_to_send: {
        break;
    }

    default:
        break;
    }

    return 0;
}

} 
} 
