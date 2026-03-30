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
 * Transport engine: event-driven packet pump that bridges the ns-3
 * event scheduler with picoquic's prepare_next_packet / incoming_packet
 * model.  One engine instance owns one QuicContext and one ns-3 UDP
 * socket.
 */

#ifndef PICOQUIC_TRANSPORT_H
#define PICOQUIC_TRANSPORT_H

#include "picoquic-connection.h"
#include "picoquic-context.h"

#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/inet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace ns3
{
namespace picoquic_ns3
{


using NewConnectionCallback = std::function<void(picoquic_cnx_t* cnx)>;


class QuicTransport
{
  public:
    
    QuicTransport(QuicContext&& ctx, Ptr<Node> node, const InetSocketAddress& local);

    ~QuicTransport();

    QuicTransport(const QuicTransport&) = delete;
    QuicTransport& operator=(const QuicTransport&) = delete;

    
    void Start();

    
    void Stop();

    
    QuicConnection* CreateClientConnection(const InetSocketAddress& peerAddr,
                                           const std::string& sni,
                                           const std::string& alpn,
                                           ApplicationCallback cb);

    
    void SetNewConnectionCallback(NewConnectionCallback cb);

    
    void SchedulePumpNow();

    
    picoquic_quic_t* GetQuicNative() const;

    QuicContext& GetContext();

  private:
    /* ---------------------------------------------------------------- */
    /*  Internal methods                                                */
    /* ---------------------------------------------------------------- */

    /// Socket receive callback.
    void HandleRead(Ptr<Socket> socket);

    
    void PumpPackets();

    
    void ScheduleNextWake();

    /* ---------------------------------------------------------------- */
    /*  Members                                                         */
    /* ---------------------------------------------------------------- */

    QuicContext m_ctx;             
    Ptr<Node> m_node;              
    Ptr<Socket> m_socket;          
    InetSocketAddress m_localAddr; 

    EventId m_pumpEvent; 
    bool m_running;      

    NewConnectionCallback m_newCnxCb; 

    static constexpr size_t kSendBufferSize = 1536;
    uint8_t m_sendBuffer[kSendBufferSize];


    std::unordered_map<picoquic_cnx_t*, std::unique_ptr<QuicConnection>> m_connections;
};

}
}

#endif 
