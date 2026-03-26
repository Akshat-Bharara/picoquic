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

#include "picoquic-sim-time.h"

namespace ns3
{
namespace picoquic_ns3
{

SimulatedTimeBridge::SimulatedTimeBridge()
    : m_simulatedTimeUs(0)
{
}

SimulatedTimeBridge&
SimulatedTimeBridge::GetInstance()
{
    static SimulatedTimeBridge instance;
    return instance;
}

void
SimulatedTimeBridge::Update()
{
    m_simulatedTimeUs = static_cast<uint64_t>(Simulator::Now().GetMicroSeconds());
}

uint64_t
SimulatedTimeBridge::GetCurrentTimeUs() const
{
    return m_simulatedTimeUs;
}

uint64_t*
SimulatedTimeBridge::GetSimulatedTimePtr()
{
    return &m_simulatedTimeUs;
}

} 
} 
