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
 * QUIC client application for ns-3 — sends a configurable amount of
 * data (bulk transfer or request/response) over a single QUIC stream,
 * then closes the connection.  Designed as a reference implementation
 * and test vehicle for the picoquic-ns3 integration.
 */

#ifndef PICOQUIC_CLIENT_APP_H
#define PICOQUIC_CLIENT_APP_H

#include "picoquic-connection.h"
#include "picoquic-context.h"
#include "picoquic-transport.h"

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/inet-socket-address.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ns3
{
namespace picoquic_ns3
{


class QuicClientApp : public Application
{
  public:
    
    static TypeId GetTypeId();

    QuicClientApp();
    ~QuicClientApp() override;

    
    void SetMaxBytes(uint64_t maxBytes);

    
    void SetRemote(const InetSocketAddress& addr);

    TracedCallback<Ptr<const QuicClientApp>> m_connectionEstablished;

    TracedCallback<Ptr<const QuicClientApp>> m_connectionClosed;

    TracedCallback<Ptr<const QuicClientApp>, uint64_t> m_txTrace;

    TracedCallback<Ptr<const QuicClientApp>> m_transferComplete;

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    
    int AppCallback(picoquic_cnx_t* cnx,
                    uint64_t streamId,
                    uint8_t* bytes,
                    size_t length,
                    picoquic_call_back_event_t event,
                    void* streamCtx);

    /* ---------------------------------------------------------------- */
    /*  Configuration attributes                                        */
    /* ---------------------------------------------------------------- */
    Address m_peerAddr;        
    std::string m_alpn;         
    std::string m_sni;          
    std::string m_certRootFile; 
    std::string m_ccAlgo;       
    std::string m_qlogDir;      
    uint64_t m_maxBytes;        
    bool m_disableCertVerify;   

    /* ---------------------------------------------------------------- */
    /*  Runtime state                                                   */
    /* ---------------------------------------------------------------- */
    std::unique_ptr<QuicTransport> m_transport; 
    QuicConnection* m_conn;                     
    uint64_t m_streamId;                        
    uint64_t m_totalSent;                      
    bool m_connected;                           
};

} 
} 

#endif 
