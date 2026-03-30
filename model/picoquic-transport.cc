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

#include "picoquic-transport.h"

#include "picoquic-sim-time.h"

#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"

#include <cstring>
#include <picoquic.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PicoquicTransport");

namespace picoquic_ns3
{

/* ================================================================== */
/*  Address conversion helpers                                        */
/* ================================================================== */


static InetSocketAddress
SockaddrToNs3Addr(const struct sockaddr_storage& ss)
{
    const auto* sin = reinterpret_cast<const struct sockaddr_in*>(&ss);
    uint32_t ipHost = ntohl(sin->sin_addr.s_addr);
    uint16_t port = ntohs(sin->sin_port);
    return InetSocketAddress(Ipv4Address(ipHost), port);
}


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

/* ================================================================== */
/*  Construction / destruction                                        */
/* ================================================================== */

QuicTransport::QuicTransport(QuicContext&& ctx, Ptr<Node> node, const InetSocketAddress& local)
    : m_ctx(std::move(ctx)),
      m_node(node),
      m_socket(nullptr),
      m_localAddr(local),
      m_running(false)
{
    NS_LOG_FUNCTION(this);
}

QuicTransport::~QuicTransport()
{
    NS_LOG_FUNCTION(this);
    Stop();
}

/* ================================================================== */
/*  Start / Stop                                                      */
/* ================================================================== */

void
QuicTransport::Start()
{
    NS_LOG_FUNCTION(this);
    if (m_running)
    {
        return;
    }
    m_running = true;

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(m_node, tid);

    int ret = m_socket->Bind(m_localAddr);
    NS_ABORT_MSG_IF(ret < 0, "Failed to bind UDP socket to " << m_localAddr);

    m_socket->SetRecvCallback(MakeCallback(&QuicTransport::HandleRead, this));

    NS_LOG_INFO("QuicTransport started on " << m_localAddr);

    SchedulePumpNow();
}

void
QuicTransport::Stop()
{
    NS_LOG_FUNCTION(this);
    m_running = false;

    if (m_pumpEvent.IsPending())
    {
        Simulator::Cancel(m_pumpEvent);
    }

    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket = nullptr;
    }

    m_connections.clear();
    NS_LOG_INFO("QuicTransport stopped");
}

/* ================================================================== */
/*  Connection management                                             */
/* ================================================================== */

QuicConnection*
QuicTransport::CreateClientConnection(const InetSocketAddress& peerAddr,
                                      const std::string& sni,
                                      const std::string& alpn,
                                      ApplicationCallback cb)
{
    NS_LOG_FUNCTION(this << peerAddr << sni << alpn);

    auto conn = std::make_unique<QuicConnection>(
        QuicConnection::CreateClientConnection(m_ctx, peerAddr, sni, alpn));
    conn->SetApplicationCallback(std::move(cb));

    picoquic_cnx_t* raw = conn->GetNative();
    QuicConnection* ptr = conn.get();
    m_connections[raw] = std::move(conn);

    ptr->Start();

    SchedulePumpNow();

    return ptr;
}

void
QuicTransport::SetNewConnectionCallback(NewConnectionCallback cb)
{
    NS_LOG_FUNCTION(this);
    m_newCnxCb = std::move(cb);
    
}

void
QuicTransport::SchedulePumpNow()
{
    if (!m_running)
    {
        return;
    }
    if (m_pumpEvent.IsPending())
    {
        Simulator::Cancel(m_pumpEvent);
    }
    m_pumpEvent = Simulator::ScheduleNow(&QuicTransport::PumpPackets, this);
}

picoquic_quic_t*
QuicTransport::GetQuicNative() const
{
    return m_ctx.GetNative();
}

QuicContext&
QuicTransport::GetContext()
{
    return m_ctx;
}

/* ================================================================== */
/*  Receive path                                                      */
/* ================================================================== */

void
QuicTransport::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Ptr<Packet> packet;
    Address fromAddr;

    while ((packet = socket->RecvFrom(fromAddr)))
    {
        if (packet->GetSize() == 0)
        {
            break;
        }

        InetSocketAddress from = InetSocketAddress::ConvertFrom(fromAddr);
        NS_LOG_LOGIC("Received " << packet->GetSize() << " bytes from " << from);

        uint32_t pktSize = packet->GetSize();
        auto buffer = std::make_unique<uint8_t[]>(pktSize);
        packet->CopyData(buffer.get(), pktSize);

        struct sockaddr_storage ssFrom = Ns3AddrToSockaddr(from);
        struct sockaddr_storage ssTo = Ns3AddrToSockaddr(m_localAddr);

        auto& bridge = SimulatedTimeBridge::GetInstance();
        bridge.Update();
        uint64_t now = bridge.GetCurrentTimeUs();

        picoquic_cnx_t* firstCnx = nullptr;

        int ret = picoquic_incoming_packet_ex(m_ctx.GetNative(),
                                              buffer.get(),
                                              static_cast<size_t>(pktSize),
                                              reinterpret_cast<struct sockaddr*>(&ssFrom),
                                              reinterpret_cast<struct sockaddr*>(&ssTo),
                                              0, 
                                              0, 
                                              &firstCnx,
                                              now);

        if (ret != 0)
        {
            NS_LOG_WARN("picoquic_incoming_packet_ex returned " << ret);
        }

        if (firstCnx != nullptr && m_connections.find(firstCnx) == m_connections.end())
        {
            auto conn = std::make_unique<QuicConnection>(firstCnx);
            m_connections[firstCnx] = std::move(conn);

            if (m_newCnxCb)
            {
                m_newCnxCb(firstCnx);
            }
            NS_LOG_INFO("New server-side connection accepted from " << from);
        }
    }

    SchedulePumpNow();
}

/* ================================================================== */
/*  Packet pump                                                       */
/* ================================================================== */

void
QuicTransport::PumpPackets()
{
    NS_LOG_FUNCTION(this);

    if (!m_running)
    {
        return;
    }

    auto& bridge = SimulatedTimeBridge::GetInstance();
    bridge.Update();
    uint64_t now = bridge.GetCurrentTimeUs();

    for (;;)
    {
        size_t sendLength = 0;
        struct sockaddr_storage addrTo;
        struct sockaddr_storage addrFrom;
        int ifIndex = 0;
        picoquic_connection_id_t logCid;
        picoquic_cnx_t* lastCnx = nullptr;
        size_t sendMsgSize = 0;

        std::memset(&addrTo, 0, sizeof(addrTo));
        std::memset(&addrFrom, 0, sizeof(addrFrom));

        int ret = picoquic_prepare_next_packet_ex(m_ctx.GetNative(),
                                                  now,
                                                  m_sendBuffer,
                                                  kSendBufferSize,
                                                  &sendLength,
                                                  &addrTo,
                                                  &addrFrom,
                                                  &ifIndex,
                                                  &logCid,
                                                  &lastCnx,
                                                  &sendMsgSize);

        if (ret != 0)
        {
            NS_LOG_WARN("picoquic_prepare_next_packet_ex returned " << ret);
            break;
        }

        if (sendLength == 0)
        {
            break;
        }

        InetSocketAddress dest = SockaddrToNs3Addr(addrTo);

        Ptr<Packet> pkt = Create<Packet>(m_sendBuffer, static_cast<uint32_t>(sendLength));
        int sent = m_socket->SendTo(pkt, 0, dest);

        if (sent < 0)
        {
            NS_LOG_WARN("Socket SendTo failed for " << sendLength << " bytes to " << dest);
        }
        else
        {
            NS_LOG_LOGIC("Sent " << sendLength << " bytes to " << dest);
        }
    }

    for (auto it = m_connections.begin(); it != m_connections.end();)
    {
        if (it->second && it->second->IsDisconnected())
        {
            NS_LOG_INFO("Removing disconnected connection");
            it = m_connections.erase(it);
        }
        else
        {
            ++it;
        }
    }

    ScheduleNextWake();
}

void
QuicTransport::ScheduleNextWake()
{
    if (!m_running)
    {
        return;
    }

    auto& bridge = SimulatedTimeBridge::GetInstance();
    bridge.Update();
    uint64_t now = bridge.GetCurrentTimeUs();

    int64_t delayUs =
        picoquic_get_next_wake_delay(m_ctx.GetNative(), now, 10000000 );

    if (delayUs <= 0)
    {
        delayUs = 1; 
    }

    if (m_pumpEvent.IsPending())
    {
        Simulator::Cancel(m_pumpEvent);
    }

    m_pumpEvent = Simulator::Schedule(MicroSeconds(static_cast<uint64_t>(delayUs)),
                                      &QuicTransport::PumpPackets,
                                      this);

    NS_LOG_LOGIC("Next pump in " << delayUs << " µs");
}

} 
} 
