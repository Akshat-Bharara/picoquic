# Picoquic Simulation Module for ns-3

## Getting Started

1. Clone the ns-3 mainline code

    ```bash
    git clone https://gitlab.com/nsnam/ns-3-dev.git
    ```

2. Change into the contrib directory

    ```bash
    cd ns-3-dev
    cd contrib
    ```

3. Clone the Picoquic Simulation Module

    ```bash
    https://gitlab.com/nitk8030113/picoquic-ns3.git
    ```

4. Clone Picoquic into the third-party directory

    ```bash
    cd ../third-party
    git clone https://github.com/private-octopus/picoquic.git
    ```

5. Configure picoquic and ns-3

   ```bash
    cd picoquic
    mkdir build && cd build
    cmake .. -DPICOQUIC_FETCH_PTLS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    make -j$(nproc)
   ```

   ```bash
    cd ../../../ # navigating back to ns-3 root
    ./ns3 clean 
    ```

    ```bash
    ./ns3 configure --enable-examples --enable-tests \
    -- -DPICOQUIC_PREFIX=third-party/picoquic # or absolute /path/to/picoquic
    ```

    ```bash
    ./ns3 build
    ```

6. Run the picoquic examples

    ```bash
    ./ns3 run picoquic-ns3-example
    ./ns3 run picoquic-ns3-quicperf
    ```
