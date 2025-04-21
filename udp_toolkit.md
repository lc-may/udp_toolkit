# UDP Network Testing Toolkit Documentation

## Overview

The UDP Network Testing Toolkit is a specialized tool for measuring UDP network performance, consisting of server and client components. It provides capabilities for measuring network latency, monitoring throughput, and supports bandwidth control and clock synchronization features.

## Features

1. **Clock Synchronization**: Implements the Cristian algorithm to synchronize clocks between client and server
2. **Network Latency Measurement**: Measures one-way transmission delay of UDP packets
3. **Throughput Monitoring**: Real-time monitoring and statistics of network throughput
4. **Bandwidth Control**: Client can control data transmission bandwidth
5. **Flexible Configuration**: Command-line parameters for customizing test parameters
6. **Debug Capabilities**: Server includes detailed debug output for troubleshooting

## Architecture

### Network Protocol

The toolkit uses two UDP ports for communication:
- **Synchronization Port (4000)**: For clock synchronization requests and responses
- **Data Port (5000)**: For test data packet transmission

### Packet Format

Data packets contain the following fields:
- **Sequence Number (seq)**: 4-byte integer
- **Send Timestamp (send_ts)**: 8-byte double-precision floating point
- **Clock Offset (offset)**: 8-byte double-precision floating point
- **Data Payload**: Remaining bytes

## Component Design

### Server Component (udp_toolkit_server.c)

The server component is responsible for:
1. Listening for sync requests and receiving data packets
2. Calculating network latency
3. Collecting real-time and average throughput statistics
4. Providing debugging information

Key modules:
- **Clock Synchronization Handler**: Responds to client clock sync requests
- **Packet Receiver**: Receives and parses data packets
- **Latency Calculator**: Calculates one-way delay based on timestamps
- **Throughput Monitor**: Calculates real-time and average throughput per second

### Client Component (udp_toolkit_client.c)

The client component is responsible for:
1. Synchronizing clocks with the server
2. Sending test data packets at specified bandwidth

Key modules:
- **Clock Synchronizer**: Implements Cristian algorithm for clock synchronization
- **Bandwidth Controller**: Precisely controls sending rate
- **Packet Generator**: Generates and sends test data packets

## Implementation Details

### Command-Line Options

The client supports the following command-line options:
- `-i IP_ADDRESS`: Specify server IP address (default: 127.0.0.1)
- `-b BANDWIDTH`: Specify sending bandwidth in bps (default: 1000000)
- `-t DURATION`: Specify test duration in seconds (default: 10)
- `-s SIZE`: Specify packet size in bytes (default: 1000)
- `-h`: Display help message

### Clock Synchronization Algorithm

Uses Cristian's algorithm for clock synchronization:
1. Client records current time t1
2. Client sends sync request to server
3. Server receives request and returns current server time t2
4. Client receives response and records time t3
5. Client calculates RTT = t3 - t1
6. Calculates clock offset: offset = t2 - (t1 + RTT/2)

### Latency Calculation

One-way latency calculation:
- Server reception time - (Client send time + Clock offset)

### Bandwidth Control

Uses nanosleep to precisely control sending intervals:
- Sending interval = (Packet size × 8) / Bandwidth

## Technical Details

1. Uses CLOCK_MONOTONIC high-precision monotonic clock
2. Uses select function for non-blocking IO multiplexing
3. Nanosecond-level time precision
4. Configurable packet size and bandwidth
5. Dynamic memory allocation for variable packet sizes

## Usage Limitations

1. IPv4 only
2. Testing parameters limited to command-line options
3. No encryption or authentication support

## Build Instructions

### Prerequisites
- C compiler (gcc/clang)
- CMake (version 3.10 or higher)
- POSIX-compliant operating system

### Building with CMake

1. Generate build files:
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

2. Compile:
   ```bash
   make
   ```

3. Install (optional):
   ```bash
   sudo make install
   ```

## Testing Plan

### Test Environment Setup

#### Requirements
- Two networked computers (one server, one client)
- Or two terminals on the same computer

#### Network Configuration
1. Determine server IP address
2. Use the `-i` option to specify the server IP

### Basic Functionality Testing

#### 1. Basic Connectivity Test
1. Start the server:
   ```bash
   ./udp_toolkit_server
   ```

2. Start the client (with default parameters):
   ```bash
   ./udp_toolkit_client
   ```

3. Verification:
   - Server receives packets and calculates latency
   - Server outputs throughput statistics every second
   - Client completes sending packets as configured

#### 2. Clock Synchronization Test
1. Observe the clock offset value output by the client
2. Verify that the latency values calculated by server are reasonable (millisecond range)

### Performance Testing

#### 1. Bandwidth Testing
Test with different bandwidth settings using the `-b` option:

1. Low bandwidth test (100 Kbps):
   ```bash
   ./udp_toolkit_client -b 100000
   ```

2. Medium bandwidth test (1 Mbps):
   ```bash
   ./udp_toolkit_client -b 1000000
   ```

3. High bandwidth test (10 Mbps):
   ```bash
   ./udp_toolkit_client -b 10000000
   ```

#### 2. Packet Size Testing
Test with different packet sizes using the `-s` option:

1. Small packet test (100 bytes):
   ```bash
   ./udp_toolkit_client -s 100
   ```

2. Medium packet test (1000 bytes):
   ```bash
   ./udp_toolkit_client -s 1000
   ```

3. Large packet test (8000 bytes):
   ```bash
   ./udp_toolkit_client -s 8000
   ```

### Network Condition Testing

#### 1. Ideal Network Environment
Test in a LAN or low-latency network environment

#### 2. Non-ideal Network Conditions
Use network simulation tools (e.g., tc, netem) to create network delay and packet loss:

1. Adding network delay:
   ```bash
   sudo tc qdisc add dev eth0 root netem delay 100ms
   ```

2. Adding packet loss:
   ```bash
   sudo tc qdisc add dev eth0 root netem loss 5%
   ```

3. Adding jitter:
   ```bash
   sudo tc qdisc add dev eth0 root netem delay 100ms 30ms
   ```

#### 3. Stress Testing
1. Run long-duration tests using the `-t` option (e.g., 3600 seconds)
2. Observe server resource usage and stability

### Test Results Analysis

Collect and analyze:
1. Network latency (average, minimum, maximum, standard deviation)
2. Throughput (average, peak)
3. Resource usage (CPU, memory)
4. Performance comparison under different network conditions

### Edge Case Testing

1. Very high bandwidth test (100 Mbps)
2. Very low bandwidth test (10 Kbps)
3. Very small packets (21 bytes - minimum supported size)
4. Very large packets (64KB, subject to network MTU limits)

### Fault Testing

1. Server starts first, client starts later
2. Client starts first, server starts later
3. Server restarts while client is running
4. Network interruption and recovery

## Sample Test Script

Here's a sample script for testing with specific parameters:

```bash
#!/bin/bash

# Sample test script for UDP toolkit
# Tests with 2Mbps bandwidth for 30 seconds with 1000-byte packets

SERVER_IP="192.168.1.100"  # Replace with actual server IP

echo "Starting UDP toolkit client test..."
./udp_toolkit_client -i $SERVER_IP -b 2000000 -t 30 -s 1000
echo "Test completed." 