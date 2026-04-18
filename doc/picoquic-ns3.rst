.. include:: replace.txt
.. highlight:: cpp

--------
picoquic
--------

********
Overview
********

The picoquic module provides an integration of the picoquic library into |ns3|. Picoquic is a fast,minimalistic implementation of the IETF QUIC standard in C. This module transparently wraps the library into native |ns3| C++ paradigms,bridging the |ns3| discrete-event scheduler and network stack with the picoquic internal state machine.

By using this module, users can simulate QUIC connections natively within their network topologies, test various congestion control algorithms (like BBR,NewReno and Cubic) dynamically provided by the picoquic library and analyze transport-layer performance.

********************
Design documentation
********************

The library integration focuses strictly on mapping picoquic internal functionalities into |ns3| objects without modifying the core PicoQUIC source code. It wraps low-level structures and callbacks,providing high-level interfaces tailored to the |ns3| workflow.

===============
Core Components
===============

QuicContext
###########

The ``QuicContext`` class is an RAII (Resource Acquisition Is Initialization) wrapper around the native ``picoquic_quic_t`` structure. This structure holds the global state for a QUIC node (or an endpoint),encompassing cryptography configurations,TLS certificates,keys,congestion control configurations and connection tracking. Any instantiation of a QUIC endpoint requires a configured context.

QuicConnection
##############

The ``QuicConnection`` class encapsulates a single, active QUIC connection representation,mapping directly to a native ``picoquic_cnx_t`` instance. It manages the connection lifecycle (handshake,active state, disconnection) and provides an easy-to-use C++ interface to push raw data into streams or datagrams. The connection leverages a generic application callback interface, ``ApplicationCallback``, which allows user-defined applications to intercept and react to specific stream events,such as data reception,stream resets and closures.

QuicTransport
#############

The ``QuicTransport`` class serves as the fundamental engine bridging the |ns3| application layer and the picoquic packet processing logic. It effectively connects the native |ns3| sockets with PicoQUIC's event loops.

* It assumes ownership of a ``QuicContext`` and a standard |ns3| UDP socket.
* It leverages the |ns3| event scheduler to step the PicoQUIC internal clock via the ``prepare_next_packet`` functionality.
* When a packet is ready and scheduled for transmission, the ``QuicTransport`` parses its bytes,wraps them in an |ns3| ``Packet`` and sends them out the UDP socket.
* On the receiving end, when the UDP socket registers incoming data, the transport extracts the bytes and forwards them into the ``picoquic`` internal function ``incoming_packet`` for parsing by the library.

============
Applications
============

The module includes ready-to-use application models that act as QUIC endpoints for testing and simulating data transfers.

QuicClientApp
#############

The ``QuicClientApp`` inherits from the standard |ns3| ``Application`` class and initiates a QUIC connection to a specified remote server address. Once the underlying ``QuicTransport`` establishes the secure connection, the client seamlessly begins a transmission over a single stream up to a configurable threshold (e.g., using the ``MaxBytes`` attribute). It offers traced callbacks to track stream events such as ``connectionEstablished``, ``connectionClosed``, and ``txTrace``. 

QuicServerApp
#############

The ``QuicServerApp`` behaves as a daemon listening on a designated port for incoming QUIC requests. Upon receiving valid packets, it routes them to internal contexts, completing the TLS handshake, authenticating certificates and establishing a ``QuicConnection``. It responds to the events managed by ``QuicTransport`` and handles incoming streams,mirroring the client architecture but designed for passive initiation.

=======
Helpers
=======

To seamlessly set up simulations across multihomed or complex |ns3| network topologies,standard |ns3| helper semantics are incorporated.

QuicServerHelper
################

The ``QuicServerHelper`` operates identically to traditional |ns3| application helpers like the ``UdpEchoServerHelper``. It is used to install one or more instances of ``QuicServerApp`` onto |ns3| nodes. It provides capabilities to bind standard |ns3| attributes across multiple nodes seamlessly,setting the listening addresses, security keys, and preferred congestion controllers in an efficient manner via a ``NodeContainer``.

QuicClientHelper
################

Similarly, the ``QuicClientHelper`` streamlines the installation of the ``QuicClientApp`` onto the simulation hosts. It manages the endpoint bindings and configures application variables prior to pushing them towards the nodes natively.
