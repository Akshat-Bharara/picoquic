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

#include "picoquic-connection.h"
#include "picoquic/tls_api.h"
#include "picoquic-sim-time.h"

#include "ns3/inet-socket-address.h"
#include "ns3/log.h"

#include <cstring>
#include <picoquic.h>
#include <utility>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PicoquicConnection");

namespace picoquic_ns3
{

/* ================================================================== */
/*  Helper: convert InetSocketAddress → sockaddr_in                   */
/* ================================================================== */

static struct sockaddr_storage
Ns3AddrToSockaddr(const InetSocketAddress& addr)
{
    struct sockaddr_storage ss;
    std::memset(&ss, 0, sizeof(ss));

    auto* sin = reinterpret_cast<struct sockaddr_in*>(&ss);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(addr.GetPort());
    sin->sin_addr.s_addr = htonl(addr.GetIpv4().Get());
    return ss;
}

int
QuicConnection::PicoquicCallbackTrampoline(picoquic_cnx_t* cnx,
                                           uint64_t streamId,
                                           uint8_t* bytes,
                                           size_t length,
                                           picoquic_call_back_event_t event,
                                           void* callbackCtx,
                                           void* streamCtx)
{
    auto* self = static_cast<QuicConnection*>(callbackCtx);
    if (self == nullptr || !self->m_appCb)
    {
        if (event == picoquic_callback_close || event == picoquic_callback_application_close ||
            event == picoquic_callback_stateless_reset)
        {
            return 0;
        }
        NS_LOG_WARN("PicoquicCallbackTrampoline: no application callback for event " << event);
        return 0;
    }
    return self->m_appCb(cnx, streamId, bytes, length, event, streamCtx);
}

/* ================================================================== */
/*  Construction helpers                                              */
/* ================================================================== */

QuicConnection::QuicConnection()
    : m_cnx(nullptr)
{
}

QuicConnection::QuicConnection(picoquic_cnx_t* cnx)
    : m_cnx(cnx)
{
}

QuicConnection::QuicConnection(QuicConnection&& other) noexcept
    : m_cnx(other.m_cnx),
      m_appCb(std::move(other.m_appCb))
{
    other.m_cnx = nullptr;
    if (m_cnx)
    {
        picoquic_set_callback(m_cnx, PicoquicCallbackTrampoline, this);
    }
}

QuicConnection&
QuicConnection::operator=(QuicConnection&& other) noexcept
{
    if (this != &other)
    {
        m_cnx = other.m_cnx;
        m_appCb = std::move(other.m_appCb);
        other.m_cnx = nullptr;
        if (m_cnx)
        {
            picoquic_set_callback(m_cnx, PicoquicCallbackTrampoline, this);
        }
    }
    return *this;
}

QuicConnection
QuicConnection::CreateClientConnection(QuicContext& ctx,
                                       const InetSocketAddress& peerAddr,
                                       const std::string& sni,
                                       const std::string& alpn)
{
    NS_LOG_FUNCTION_NOARGS();

    auto& bridge = SimulatedTimeBridge::GetInstance();
    bridge.Update();
    uint64_t now = bridge.GetCurrentTimeUs();

    struct sockaddr_storage ss = Ns3AddrToSockaddr(peerAddr);

    picoquic_cnx_t* cnx =
        picoquic_create_client_cnx(ctx.GetNative(),
                                   reinterpret_cast<struct sockaddr*>(&ss),
                                   now,
                                   0, /* preferred_version = latest */
                                   sni.empty() ? nullptr : sni.c_str(),
                                   alpn.empty() ? nullptr : alpn.c_str(),
                                   PicoquicCallbackTrampoline,
                                   nullptr /* callback_ctx – set in SetApplicationCallback */
        );

    if (cnx == nullptr)
    {
        NS_FATAL_ERROR("picoquic_create_client_cnx() failed");
    }

    QuicConnection conn(cnx);
    NS_LOG_INFO("Client connection created to " << peerAddr);
    return conn;
}

/* ================================================================== */
/*  Callback registration                                             */
/* ================================================================== */

void
QuicConnection::SetApplicationCallback(ApplicationCallback cb)
{
    NS_LOG_FUNCTION(this);
    m_appCb = std::move(cb);
    if (m_cnx)
    {
        picoquic_set_callback(m_cnx, PicoquicCallbackTrampoline, this);
    }
}

/* ================================================================== */
/*  Connection lifecycle                                              */
/* ================================================================== */

int
QuicConnection::Start()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_start_client_cnx(m_cnx);
}

int
QuicConnection::Close(uint64_t reasonCode)
{
    NS_LOG_FUNCTION(this << reasonCode);
    if (!m_cnx)
    {
        return -1;
    }
    return picoquic_close(m_cnx, reasonCode);
}

void
QuicConnection::CloseImmediate()
{
    NS_LOG_FUNCTION(this);
    if (m_cnx)
    {
        picoquic_close_immediate(m_cnx);
    }
}

/* ================================================================== */
/*  Stream API                                                        */
/* ================================================================== */

int
QuicConnection::AddToStream(uint64_t streamId, const uint8_t* data, size_t length, bool isFin)
{
    NS_LOG_FUNCTION(this << streamId << length << isFin);
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_add_to_stream(m_cnx, streamId, data, length, isFin ? 1 : 0);
}

int
QuicConnection::MarkActiveStream(uint64_t streamId, bool isActive, void* streamCtx)
{
    NS_LOG_FUNCTION(this << streamId << isActive);
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_mark_active_stream(m_cnx, streamId, isActive ? 1 : 0, streamCtx);
}

int
QuicConnection::ResetStream(uint64_t streamId, uint64_t errorCode)
{
    NS_LOG_FUNCTION(this << streamId << errorCode);
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_reset_stream(m_cnx, streamId, errorCode);
}

int
QuicConnection::StopSending(uint64_t streamId, uint64_t errorCode)
{
    NS_LOG_FUNCTION(this << streamId << errorCode);
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_stop_sending(m_cnx, streamId, errorCode);
}

uint64_t
QuicConnection::GetNextLocalStreamId(bool isUnidirectional) const
{
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_get_next_local_stream_id(m_cnx, isUnidirectional ? 1 : 0);
}

/* ================================================================== */
/*  Datagram API                                                      */
/* ================================================================== */

int
QuicConnection::QueueDatagram(const uint8_t* data, size_t length)
{
    NS_LOG_FUNCTION(this << length);
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_queue_datagram_frame(m_cnx, length, data);
}

int
QuicConnection::MarkDatagramReady(bool isReady)
{
    NS_LOG_FUNCTION(this << isReady);
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_mark_datagram_ready(m_cnx, isReady ? 1 : 0);
}

/* ================================================================== */
/*  State queries                                                     */
/* ================================================================== */

picoquic_state_enum
QuicConnection::GetState() const
{
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_get_cnx_state(m_cnx);
}

bool
QuicConnection::IsReady() const
{
    if (!m_cnx)
    {
        return false;
    }
    auto st = picoquic_get_cnx_state(m_cnx);
    return (st == picoquic_state_ready || st == picoquic_state_client_ready_start);
}

bool
QuicConnection::IsDisconnected() const
{
    if (!m_cnx)
    {
        return true;
    }
    auto st = picoquic_get_cnx_state(m_cnx);
    return (st >= picoquic_state_disconnecting);
}

bool
QuicConnection::IsClient() const
{
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_is_client(m_cnx) != 0;
}

const char*
QuicConnection::GetNegotiatedAlpn() const
{
    if (!m_cnx)
    {
        return nullptr;
    }
    return picoquic_tls_get_negotiated_alpn(m_cnx);
}

picoquic_cnx_t*
QuicConnection::GetNative() const
{
    return m_cnx;
}

QuicConnection::operator bool() const
{
    return m_cnx != nullptr;
}

/* ================================================================== */
/*  Path quality                                                      */
/* ================================================================== */

uint64_t
QuicConnection::GetRtt() const
{
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_get_rtt(m_cnx);
}

uint64_t
QuicConnection::GetPacingRate() const
{
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_get_pacing_rate(m_cnx);
}

uint64_t
QuicConnection::GetCwin() const
{
    NS_ASSERT_MSG(m_cnx, "Connection not initialised");
    return picoquic_get_cwin(m_cnx);
}

} 
} 
