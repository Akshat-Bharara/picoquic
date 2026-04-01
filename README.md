# Picoquic Simulation Module for ns-3

# Getting Started

1. Clone the ns-3 mainline code

    ```bash
    git clone https://gitlab.com/nsnam/ns-3-dev.git
    ```

2. Change into the contrib directory

    ```bash
    cd contrib
    ```

3. Clone the Picoquic Simulation Module

    ```bash
    git clone https://gitlab.com/nitk8030113/picoquic.git
    ```

4. Configure picoquic and ns-3



   ```bash
    cd /path/to/picoquic
    cmake -DPICOQUIC_FETCH_PTLS=Y .
    make
    mkdir build && cd build
    cmake .. -DPICOQUIC_FETCH_PTLS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    make -j$(nproc)
   ```

   ```bash
    ./ns3 clean 
    ```

    ```bash
    ./ns3 configure --enable-examples --enable-tests \
    -- -DPICOQUIC_PREFIX=/path/to/picoquic
    ```

    ```bash
    ./ns3 build
    ```

6. Run the picoquic examples

    ```bash
    ./ns3 run picoquic-ns3-example
    ./ns3 run picoquic-ns3-quicperf
    ```
