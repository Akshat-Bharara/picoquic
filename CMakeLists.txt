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

set(PICOQUIC_PREFIX "" CACHE PATH "Path to picoquic source/build tree")

if("${PICOQUIC_PREFIX}" STREQUAL "")
    set(PICOQUIC_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/../../pico/picoquic"
        CACHE PATH "Path to picoquic source/build tree" FORCE)
endif()

message(STATUS "[picoquic-ns3] PICOQUIC_PREFIX = ${PICOQUIC_PREFIX}")

# --- Header directories ---
set(PICOQUIC_INCLUDE_DIRS
    ${PICOQUIC_PREFIX}/picoquic
    ${PICOQUIC_PREFIX}/loglib
    ${PICOQUIC_PREFIX}
)

# --- Library search paths (support both in-tree and out-of-tree builds) ---
set(PICOQUIC_LIB_SEARCH_PATHS
    ${PICOQUIC_PREFIX}/build
    ${PICOQUIC_PREFIX}/build/Release
    ${PICOQUIC_PREFIX}/build/Debug
    ${PICOQUIC_PREFIX}
)

find_library(PICOQUIC_CORE_LIB
    NAMES picoquic-core
    PATHS ${PICOQUIC_LIB_SEARCH_PATHS}
    NO_DEFAULT_PATH
)
find_library(PICOQUIC_LOG_LIB
    NAMES picoquic-log
    PATHS ${PICOQUIC_LIB_SEARCH_PATHS}
    NO_DEFAULT_PATH
)
find_library(PICOHTTP_CORE_LIB
    NAMES picohttp-core
    PATHS ${PICOQUIC_LIB_SEARCH_PATHS}
    NO_DEFAULT_PATH
)

if(NOT PICOQUIC_CORE_LIB)
    message(WARNING "[picoquic-ns3] picoquic-core library not found in "
                    "${PICOQUIC_LIB_SEARCH_PATHS}.  "
                    "Please build picoquic first or set PICOQUIC_PREFIX.")
endif()
if(NOT PICOQUIC_LOG_LIB)
    message(WARNING "[picoquic-ns3] picoquic-log library not found.  "
                    "QLOG support will be unavailable.")
endif()

# --- picotls (transitive dependency of picoquic) ---

find_path(PTLS_INCLUDE_DIR
    NAMES picotls.h
    PATH_SUFFIXES include
    HINTS ${PICOQUIC_PREFIX}/build/_deps/picotls-src/include
          ${PICOQUIC_PREFIX}/_deps/picotls-src/include
          ${PICOQUIC_PREFIX}/../picotls/include
          ${PICOQUIC_PREFIX}/picotls/include
          /usr/local/include
          /usr/include
)
find_library(PTLS_CORE_LIB
    NAMES picotls-core
    HINTS ${PICOQUIC_PREFIX}/build/_deps/picotls-build
          ${PICOQUIC_PREFIX}/_deps/picotls-build
          ${PICOQUIC_PREFIX}/../picotls/build
          ${PICOQUIC_PREFIX}/picotls/build
          /usr/local/lib
          /usr/lib
)
find_library(PTLS_OPENSSL_LIB
    NAMES picotls-openssl
    HINTS ${PICOQUIC_PREFIX}/build/_deps/picotls-build
          ${PICOQUIC_PREFIX}/_deps/picotls-build
          ${PICOQUIC_PREFIX}/../picotls/build
          ${PICOQUIC_PREFIX}/picotls/build
          /usr/local/lib
          /usr/lib
)
find_library(PTLS_FUSION_LIB
    NAMES picotls-fusion
    HINTS ${PICOQUIC_PREFIX}/build/_deps/picotls-build
          ${PICOQUIC_PREFIX}/_deps/picotls-build
          ${PICOQUIC_PREFIX}/../picotls/build
          ${PICOQUIC_PREFIX}/picotls/build
          /usr/local/lib
          /usr/lib
)
find_library(PTLS_MINICRYPTO_LIB
    NAMES picotls-minicrypto
    HINTS ${PICOQUIC_PREFIX}/build/_deps/picotls-build
          ${PICOQUIC_PREFIX}/_deps/picotls-build
          ${PICOQUIC_PREFIX}/../picotls/build
          ${PICOQUIC_PREFIX}/picotls/build
          /usr/local/lib
          /usr/lib
)

find_package(OpenSSL QUIET)
find_package(Threads QUIET)

# --- Diagnostic output ---
message(STATUS "[picoquic-ns3] PICOQUIC_CORE_LIB = ${PICOQUIC_CORE_LIB}")
message(STATUS "[picoquic-ns3] PICOQUIC_LOG_LIB  = ${PICOQUIC_LOG_LIB}")
message(STATUS "[picoquic-ns3] PICOHTTP_CORE_LIB = ${PICOHTTP_CORE_LIB}")
message(STATUS "[picoquic-ns3] PTLS_INCLUDE_DIR  = ${PTLS_INCLUDE_DIR}")
message(STATUS "[picoquic-ns3] PTLS_CORE_LIB     = ${PTLS_CORE_LIB}")
message(STATUS "[picoquic-ns3] PTLS_OPENSSL_LIB  = ${PTLS_OPENSSL_LIB}")
message(STATUS "[picoquic-ns3] PTLS_FUSION_LIB   = ${PTLS_FUSION_LIB}")
message(STATUS "[picoquic-ns3] PTLS_MINICRYPTO   = ${PTLS_MINICRYPTO_LIB}")

if(PICOQUIC_CORE_LIB)
    add_library(picoquic::core STATIC IMPORTED)
    set_target_properties(picoquic::core PROPERTIES
        IMPORTED_LOCATION "${PICOQUIC_CORE_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${PICOQUIC_INCLUDE_DIRS}")

    set(_pq_link_libs "")
    if(PTLS_CORE_LIB)
        list(APPEND _pq_link_libs ${PTLS_CORE_LIB})
    endif()
    if(PTLS_OPENSSL_LIB)
        list(APPEND _pq_link_libs ${PTLS_OPENSSL_LIB})
    endif()
    if(PTLS_FUSION_LIB)
        list(APPEND _pq_link_libs ${PTLS_FUSION_LIB})
    endif()
    if(PTLS_MINICRYPTO_LIB)
        list(APPEND _pq_link_libs ${PTLS_MINICRYPTO_LIB})
    endif()
    if(OPENSSL_FOUND)
        list(APPEND _pq_link_libs OpenSSL::SSL OpenSSL::Crypto)
    endif()
    if(Threads_FOUND)
        list(APPEND _pq_link_libs Threads::Threads)
    endif()
    set_property(TARGET picoquic::core APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES "${_pq_link_libs}")
endif()

if(PICOQUIC_LOG_LIB)
    add_library(picoquic::log STATIC IMPORTED)
    set_target_properties(picoquic::log PROPERTIES
        IMPORTED_LOCATION "${PICOQUIC_LOG_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${PICOQUIC_INCLUDE_DIRS}")
    if(PICOQUIC_CORE_LIB)
        set_property(TARGET picoquic::log APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES picoquic::core)
    endif()
endif()

# picohttp-core provides the quicperf protocol (quicperf.h / quicperf.c).
if(PICOHTTP_CORE_LIB)
    add_library(picoquic::http STATIC IMPORTED)
    set_target_properties(picoquic::http PROPERTIES
        IMPORTED_LOCATION "${PICOHTTP_CORE_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${PICOQUIC_PREFIX}/picohttp")
    if(PICOQUIC_CORE_LIB)
        set_property(TARGET picoquic::http APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES picoquic::core)
    endif()
endif()

set(PICOQUIC_NS3_EXTERNAL_LIBS "")
if(TARGET picoquic::core)
    list(APPEND PICOQUIC_NS3_EXTERNAL_LIBS picoquic::core)
endif()
if(TARGET picoquic::log)
    list(APPEND PICOQUIC_NS3_EXTERNAL_LIBS picoquic::log)
endif()
if(TARGET picoquic::http)
    list(APPEND PICOQUIC_NS3_EXTERNAL_LIBS picoquic::http)
endif()

build_lib(
    LIBNAME picoquic-ns3
    SOURCE_FILES
        model/picoquic-sim-time.cc
        model/picoquic-context.cc
        model/picoquic-connection.cc
        model/picoquic-transport.cc
        model/picoquic-client-app.cc
        model/picoquic-server-app.cc
        helper/picoquic-helper.cc
    HEADER_FILES
        model/picoquic-types-fwd.h
        model/picoquic-sim-time.h
        model/picoquic-context.h
        model/picoquic-connection.h
        model/picoquic-transport.h
        model/picoquic-client-app.h
        model/picoquic-server-app.h
        helper/picoquic-helper.h
    LIBRARIES_TO_LINK
        ${libinternet}
        ${libpoint-to-point}
        ${PICOQUIC_NS3_EXTERNAL_LIBS}
    TEST_SOURCES
        test/picoquic-ns3-test.cc
)

if(PICOQUIC_INCLUDE_DIRS)
    target_include_directories(picoquic-ns3
        PRIVATE ${PICOQUIC_INCLUDE_DIRS})
    if(PTLS_INCLUDE_DIR)
        target_include_directories(picoquic-ns3
            PRIVATE ${PTLS_INCLUDE_DIR})
    endif()

    target_compile_definitions(picoquic-ns3
        PRIVATE "PICOQUIC_NS3_PICOQUIC_PREFIX=\"${PICOQUIC_PREFIX}\"")

    if(TARGET picoquic-ns3-test)
        target_include_directories(picoquic-ns3-test
            PRIVATE ${PICOQUIC_INCLUDE_DIRS})
        if(PTLS_INCLUDE_DIR)
            target_include_directories(picoquic-ns3-test
                PRIVATE ${PTLS_INCLUDE_DIR})
        endif()
        target_compile_definitions(picoquic-ns3-test
            PRIVATE "PICOQUIC_NS3_PICOQUIC_PREFIX=\"${PICOQUIC_PREFIX}\"")
    endif()
endif()

