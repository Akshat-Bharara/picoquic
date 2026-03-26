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

#include "picoquic-context.h"

#include "picoquic-sim-time.h"

#include "ns3/log.h"

#include <autoqlog.h>
#include <picoquic.h>
#include <picoquic_utils.h>
#include <stdexcept>
#include <utility>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PicoquicContext");

namespace picoquic_ns3
{

/* ------------------------------------------------------------------ */
/*  Construction / destruction                                        */
/* ------------------------------------------------------------------ */

QuicContext::QuicContext(const QuicContextConfig& config)
    : m_quic(nullptr)
{
    NS_LOG_FUNCTION(this);

    auto& bridge = SimulatedTimeBridge::GetInstance();
    bridge.Update();
    uint64_t now = bridge.GetCurrentTimeUs();

    picoquic_register_all_congestion_control_algorithms();

    m_quic = picoquic_create(config.maxConnections,
                             config.certFile,
                             config.keyFile,
                             config.certRootFile,
                             config.defaultAlpn,
                             nullptr, /* default_callback_fn  – set per-connection */
                             nullptr, /* default_callback_ctx – set per-connection */
                             nullptr, /* cnx_id_callback */
                             nullptr, /* cnx_id_callback_data */
                             nullptr, /* reset_seed */
                             now,
                             bridge.GetSimulatedTimePtr(),
                             config.ticketFile,
                             nullptr, /* ticket_encryption_key */
                             0        /* ticket_encryption_key_length */
    );

    if (m_quic == nullptr)
    {
        NS_FATAL_ERROR("picoquic_create() returned nullptr");
    }

    if (!config.ccAlgorithm.empty())
    {
        SetCongestionAlgorithm(config.ccAlgorithm);
    }
    if (config.idleTimeoutMs > 0)
    {
        SetIdleTimeout(config.idleTimeoutMs);
    }
    if (config.disableCertVerification)
    {
        DisableCertificateVerification();
    }

    NS_LOG_INFO("PicoquicContext created (maxConn="
                << config.maxConnections
                << ", alpn=" << (config.defaultAlpn ? config.defaultAlpn : "(null)")
                << ", cc=" << (config.ccAlgorithm.empty() ? "default" : config.ccAlgorithm) << ")");
}

QuicContext::QuicContext(QuicContext&& other) noexcept
    : m_quic(other.m_quic)
{
    other.m_quic = nullptr;
}

QuicContext&
QuicContext::operator=(QuicContext&& other) noexcept
{
    if (this != &other)
    {
        if (m_quic)
        {
            picoquic_free(m_quic);
        }
        m_quic = other.m_quic;
        other.m_quic = nullptr;
    }
    return *this;
}

QuicContext::~QuicContext()
{
    NS_LOG_FUNCTION(this);
    if (m_quic)
    {
        picoquic_free(m_quic);
        m_quic = nullptr;
    }
}

/* ------------------------------------------------------------------ */
/*  Accessors                                                         */
/* ------------------------------------------------------------------ */

picoquic_quic_t*
QuicContext::GetNative() const
{
    return m_quic;
}

QuicContext::operator bool() const
{
    return m_quic != nullptr;
}

/* ------------------------------------------------------------------ */
/*  Configuration helpers                                             */
/* ------------------------------------------------------------------ */

void
QuicContext::SetDefaultTransportParameters(const picoquic_tp_t& tp)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(m_quic, "QuicContext not initialised");
    picoquic_set_default_tp(m_quic, const_cast<picoquic_tp_t*>(&tp));
}

void
QuicContext::SetCongestionAlgorithm(const std::string& name)
{
    NS_LOG_FUNCTION(this << name);
    NS_ASSERT_MSG(m_quic, "QuicContext not initialised");
    const picoquic_congestion_algorithm_t* algo = picoquic_get_congestion_algorithm(name.c_str());
    if (algo == nullptr)
    {
        NS_LOG_WARN("Unknown congestion algorithm '" << name << "'; keeping default.");
        return;
    }
    picoquic_set_default_congestion_algorithm(m_quic, algo);
}

void
QuicContext::SetIdleTimeout(uint64_t timeoutMs)
{
    NS_LOG_FUNCTION(this << timeoutMs);
    NS_ASSERT_MSG(m_quic, "QuicContext not initialised");
    picoquic_set_default_idle_timeout(m_quic, timeoutMs);
}

void
QuicContext::DisableCertificateVerification()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(m_quic, "QuicContext not initialised");
    picoquic_set_null_verifier(m_quic);
}

void
QuicContext::EnableQlog(const std::string& qlogDir)
{
    NS_LOG_FUNCTION(this << qlogDir);
    NS_ASSERT_MSG(m_quic, "QuicContext not initialised");
    picoquic_set_qlog(m_quic, qlogDir.c_str());
    picoquic_set_log_level(m_quic, 1);
}

void
QuicContext::SetMtuMax(uint32_t mtuMax)
{
    NS_LOG_FUNCTION(this << mtuMax);
    NS_ASSERT_MSG(m_quic, "QuicContext not initialised");
    picoquic_set_mtu_max(m_quic, mtuMax);
}

void
QuicContext::AdjustMaxConnections(uint32_t n)
{
    NS_LOG_FUNCTION(this << n);
    NS_ASSERT_MSG(m_quic, "QuicContext not initialised");
    picoquic_adjust_max_connections(m_quic, n);
}

} 
} 
