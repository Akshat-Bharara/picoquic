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
 * Forward declarations and common type aliases for the picoquic C API.
 * This header is intended for use by the picoquic-ns3 public headers
 * so that downstream users do not need to include the full picoquic.h.
 *
 * IMPORTANT: This header does NOT include picoquic.h.  It only provides
 * opaque pointer types.  The enum values are intentionally excluded
 * from this file — implementation files include picoquic.h directly.
 */

#ifndef PICOQUIC_TYPES_FWD_H
#define PICOQUIC_TYPES_FWD_H

#include <cstdint>

extern "C"
{
    typedef struct st_picoquic_quic_t picoquic_quic_t;
    typedef struct st_picoquic_cnx_t picoquic_cnx_t;
    typedef struct st_picoquic_path_t picoquic_path_t;
    typedef struct st_picoquic_tp_t picoquic_tp_t;
    typedef struct st_picoquic_congestion_algorithm_t picoquic_congestion_algorithm_t;
}

#endif /* PICOQUIC_TYPES_FWD_H */
