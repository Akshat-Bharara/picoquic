# Copyright (c) 2026 NITK Surathkal
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Authors: Akshat Bharara <akshat.bharara05@gmail.com>
#          Samved Sajankila <samvedsajankila58117@gmail.com>
#          Sanjay S Bhat <sanjay95bhat@gmail.com>
#          Jeferson Provalto <pravaltojeferson@gmail.com>
#          Swaraj Singh <swaraj050605@gmail.com>
#          Mohit P. Tahiliani <tahiliani@nitk.edu.in>

build_lib_example(
    NAME picoquic-ns3-example
    SOURCE_FILES picoquic-ns3-example.cc
    LIBRARIES_TO_LINK
        ${libpicoquic-ns3}
        ${libinternet}
        ${libpoint-to-point}
        ${libapplications}
)

# Pass the picoquic prefix for locating test certificates at runtime.
if(TARGET picoquic-ns3-example AND PICOQUIC_PREFIX)
    target_compile_definitions(picoquic-ns3-example
        PRIVATE "PICOQUIC_NS3_PICOQUIC_PREFIX=\"${PICOQUIC_PREFIX}\"")
endif()

if(PICOHTTP_CORE_LIB)
    build_lib_example(
        NAME picoquic-ns3-quicperf
        SOURCE_FILES picoquic-ns3-quicperf.cc
        LIBRARIES_TO_LINK
            ${libpicoquic-ns3}
            ${libinternet}
            ${libpoint-to-point}
            ${libapplications}
            picoquic::http
    )

    if(TARGET picoquic-ns3-quicperf AND PICOQUIC_PREFIX)
        target_compile_definitions(picoquic-ns3-quicperf
            PRIVATE "PICOQUIC_NS3_PICOQUIC_PREFIX=\"${PICOQUIC_PREFIX}\"")
    endif()
endif()
