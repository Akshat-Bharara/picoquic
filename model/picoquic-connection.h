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
 * C++ wrapper around a picoquic QUIC connection (picoquic_cnx_t).
 */

#ifndef PICOQUIC_CONNECTION_H
#define PICOQUIC_CONNECTION_H

#include "picoquic-context.h"

#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/inet-socket-address.h"
#include "ns3/log.h"

#include <cstdint>
#include <functional>
#include <picoquic.h>
#include <string>

namespace ns3
{
namespace picoquic_ns3
{

using ApplicationCallback = std::function<int(picoquic_cnx_t* cnx,
                                              uint64_t streamId,
                                              uint8_t* bytes,
                                              size_t length,
                                              picoquic_call_back_event_t event,
                                              void* streamCtx)>;

class QuicConnection
{
  public:
    static QuicConnection CreateClientConnection(QuicContext& ctx,
                                                 const InetSocketAddress& peerAddr,
                                                 const std::string& sni,
                                                 const std::string& alpn);

    explicit QuicConnection(picoquic_cnx_t* cnx);

    QuicConnection();

    QuicConnection(const QuicConnection&) = delete;
    QuicConnection& operator=(const QuicConnection&) = delete;

    QuicConnection(QuicConnection&& other) noexcept;
    QuicConnection& operator=(QuicConnection&& other) noexcept;

    void SetApplicationCallback(ApplicationCallback cb);

    int Start();

    int Close(uint64_t reasonCode = 0);

    void CloseImmediate();

    int AddToStream(uint64_t streamId, const uint8_t* data, size_t length, bool isFin = false);

    int MarkActiveStream(uint64_t streamId, bool isActive, void* streamCtx = nullptr);

    int ResetStream(uint64_t streamId, uint64_t errorCode);

    int StopSending(uint64_t streamId, uint64_t errorCode);

    uint64_t GetNextLocalStreamId(bool isUnidirectional) const;

    int QueueDatagram(const uint8_t* data, size_t length);

    int MarkDatagramReady(bool isReady);

    picoquic_state_enum GetState() const;

    bool IsReady() const;

    bool IsDisconnected() const;

    bool IsClient() const;

    const char* GetNegotiatedAlpn() const;

    picoquic_cnx_t* GetNative() const;

    explicit operator bool() const;

    uint64_t GetRtt() const;

    uint64_t GetPacingRate() const;

    uint64_t GetCwin() const;

    static int PicoquicCallbackTrampoline(picoquic_cnx_t* cnx,
                                          uint64_t streamId,
                                          uint8_t* bytes,
                                          size_t length,
                                          picoquic_call_back_event_t event,
                                          void* callbackCtx,
                                          void* streamCtx);

  private:
    picoquic_cnx_t* m_cnx;
    ApplicationCallback m_appCb;
};

} 
} 

#endif 
