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
 * Integration of picoquic QUIC library with ns-3 network simulator.
 * Simulated time bridge: maps ns-3 Simulator::Now() to picoquic's
 * p_simulated_time pointer used throughout the QUIC stack.
 */

#ifndef PICOQUIC_SIM_TIME_H
#define PICOQUIC_SIM_TIME_H

#include "ns3/nstime.h"
#include "ns3/simulator.h"

#include <cstdint>

namespace ns3
{
namespace picoquic_ns3
{

class SimulatedTimeBridge
{
  public:
    static SimulatedTimeBridge& GetInstance();

    SimulatedTimeBridge(const SimulatedTimeBridge&) = delete;
    SimulatedTimeBridge& operator=(const SimulatedTimeBridge&) = delete;

    void Update();

    uint64_t GetCurrentTimeUs() const;

    uint64_t* GetSimulatedTimePtr();

  private:
    SimulatedTimeBridge();
    ~SimulatedTimeBridge() = default;

    uint64_t m_simulatedTimeUs; 
};

} 
} 

#endif 
