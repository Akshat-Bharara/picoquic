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
 * C++ wrapper around the picoquic QUIC context (picoquic_quic_t).
 * Manages context lifetime, default callbacks, congestion-control
 * selection, and transport-parameter defaults.
 */

#ifndef PICOQUIC_CONTEXT_H
#define PICOQUIC_CONTEXT_H

#include "picoquic-types-fwd.h"

#include "ns3/log.h"
#include "ns3/nstime.h"

#include <cstdint>
#include <functional>
#include <string>

namespace ns3
{
namespace picoquic_ns3
{

struct QuicContextConfig
{
    uint32_t maxConnections{256};
    const char* certFile{nullptr};
    const char* keyFile{nullptr};
    const char* certRootFile{nullptr};
    const char* defaultAlpn{nullptr};
    const char* ticketFile{nullptr};
    std::string ccAlgorithm;
    uint64_t idleTimeoutMs{0};
    bool disableCertVerification{false};
};

class QuicContext
{
  public:
    explicit QuicContext(const QuicContextConfig& config);

    QuicContext(const QuicContext&) = delete;
    QuicContext& operator=(const QuicContext&) = delete;

    QuicContext(QuicContext&& other) noexcept;
    QuicContext& operator=(QuicContext&& other) noexcept;

    ~QuicContext();

    picoquic_quic_t* GetNative() const;

    explicit operator bool() const;

    void SetDefaultTransportParameters(const picoquic_tp_t& tp);

    void SetCongestionAlgorithm(const std::string& name);

    void SetIdleTimeout(uint64_t timeoutMs);

    void DisableCertificateVerification();

    void EnableQlog(const std::string& qlogDir);

    void SetMtuMax(uint32_t mtuMax);

    void AdjustMaxConnections(uint32_t n);

  private:
    picoquic_quic_t* m_quic;
};

} 
} 

#endif 
