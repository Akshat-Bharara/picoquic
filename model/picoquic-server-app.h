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
 * QUIC server application for ns-3 — accepts incoming QUIC connections,
 * receives stream data, and optionally echoes it back.  Designed as a
 * reference implementation for the picoquic-ns3 integration.
 */

#ifndef PICOQUIC_SERVER_APP_H
#define PICOQUIC_SERVER_APP_H

#include "picoquic-connection.h"
#include "picoquic-context.h"
#include "picoquic-transport.h"

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/inet-socket-address.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace ns3
{
namespace picoquic_ns3
{


struct ServerStreamCtx
{
    uint64_t streamId{0};
    uint64_t bytesReceived{0};
    bool finReceived{false};
};


class QuicServerApp : public Application
{
  public:
    static TypeId GetTypeId();

    QuicServerApp();
    ~QuicServerApp() override;

    TracedCallback<Ptr<const QuicServerApp>> m_connectionAccepted;
    TracedCallback<Ptr<const QuicServerApp>> m_connectionClosed;
    TracedCallback<Ptr<const QuicServerApp>, uint64_t> m_rxTrace;

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    
    void OnNewConnection(picoquic_cnx_t* cnx);

    
    static int AppCallback(picoquic_cnx_t* cnx,
                           uint64_t streamId,
                           uint8_t* bytes,
                           size_t length,
                           picoquic_call_back_event_t event,
                           void* callbackCtx,
                           void* streamCtx);

    /* ---------------------------------------------------------------- */
    /*  Configuration attributes                                        */
    /* ---------------------------------------------------------------- */
    Address m_listenAddr; 
    std::string m_certFile;
    std::string m_keyFile;
    std::string m_alpn;
    std::string m_ccAlgo;
    std::string m_qlogDir; 
    bool m_echoMode;

    /* ---------------------------------------------------------------- */
    /*  Runtime state                                                   */
    /* ---------------------------------------------------------------- */
    std::unique_ptr<QuicTransport> m_transport;
};

} 
} 

#endif 
