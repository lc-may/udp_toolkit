# UDP Network Testing Toolkit Documentation

## Overview

The UDP Network Testing Toolkit is a specialized tool for measuring UDP network performance, consisting of server and client components. It provides capabilities for measuring network latency, monitoring throughput, and supports bandwidth control and clock synchronization features. The toolkit also includes log parsing and data visualization tools.

## Features

1. **Clock Synchronization**: Implements the NTP algorithm to synchronize clocks between client and server
2. **Network Latency Measurement**: Measures one-way transmission delay of UDP packets
3. **Throughput Monitoring**: Real-time monitoring and statistics of network throughput
4. **Bandwidth Control**: Client can control data transmission bandwidth
5. **Flexible Configuration**: Command-line parameters for customizing test parameters
6. **Debug Capabilities**: Server includes detailed debug output for troubleshooting
7. **Log Analysis**: Python script for analyzing log files to extract metrics
8. **Data Visualization**: Generates latency histograms and throughput graphs

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
- **Clock Synchronizer**: Implements NTP algorithm for clock synchronization
- **Bandwidth Controller**: Precisely controls sending rate
- **Packet Generator**: Generates and sends test data packets

### Log Analysis Component (parse_logs.py)

The log analysis script provides:
1. Parsing of server debug logs
2. Packet loss analysis
3. Latency statistics calculation
4. Throughput analysis
5. Data visualization through graphs

Key features:
- **Packet Loss Detection**: Identifies and reports lost packets by sequence analysis
- **Latency Statistics**: Calculates mean, variance, minimum, and maximum latency
- **Throughput Calculation**: Computes overall and per-second throughput
- **Graph Generation**: Creates latency histogram and throughput graphs

## Implementation Details

### Command-Line Options

The client supports the following command-line options:
- `-i IP_ADDRESS`: Specify server IP address (default: 127.0.0.1)
- `-b BANDWIDTH`: Specify sending bandwidth in bps (default: 1000000)
- `-t DURATION`: Specify test duration in seconds (default: 10)
- `-s SIZE`: Specify packet size in bytes (default: 1000)
- `-h`: Display help message

The log analyzer supports the following command-line options:
- `--log-file FILE`: Specify the log file to analyze (default: server_debug_20250420_225135.log)
- `--packet-size SIZE`: Specify the packet size in bytes (default: 1000)

### Clock Synchronization Algorithm

Uses NTP algorithm for clock synchronization:
1. Client records current time t1
2. Client sends sync request to server
3. Server records receive time t2
4. Server records send time t3 and returns both t2 and t3
5. Client receives response and records time t4
6. Client calculates delay = (t4 - t1) - (t3 - t2)
7. Calculates clock offset: offset = ((t2 - t1) + (t3 - t4)) / 2.0

### Latency Calculation

One-way latency calculation:
- Server reception time - (Client send time + Clock offset)

### Bandwidth Control

Uses nanosleep to precisely control sending intervals:
- Sending interval = (Packet size × 8) / Bandwidth

### Log Analysis Functions

The log parser provides the following analyses:
- **Packet Loss Analysis**: Calculates loss rate and identifies lost packet sequences
- **Latency Analysis**: Computes statistical metrics for packet latency
- **Throughput Analysis**: Calculates overall and per-second throughput
- **Visualization**: Generates histograms and graphs for visual analysis

## Technical Details

1. Uses CLOCK_MONOTONIC high-precision monotonic clock
2. Uses select function for non-blocking IO multiplexing
3. Nanosecond-level time precision
4. Configurable packet size and bandwidth
5. Dynamic memory allocation for variable packet sizes
6. Matplotlib visualization for data analysis

## Usage Limitations

1. IPv4 only
2. Testing parameters limited to command-line options
3. No encryption or authentication support

## Build Instructions

### Prerequisites
- C compiler (gcc/clang)
- CMake (version 3.10 or higher)
- POSIX-compliant operating system
- Python 3.x with numpy and matplotlib (for log analysis)

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
   ./run_server.sh
   ```

2. Start the client (with default parameters):
   ```bash
   ./build/udp_toolkit_client
   ```

3. Start a remote test:
   ```bash
   ./remote_test.sh
   ```

4. Verification:
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
   ./build/udp_toolkit_client -b 100000
   ```

2. Medium bandwidth test (1 Mbps):
   ```bash
   ./build/udp_toolkit_client -b 1000000
   ```

3. High bandwidth test (10 Mbps):
   ```bash
   ./build/udp_toolkit_client -b 10000000
   ```

#### 2. Packet Size Testing
Test with different packet sizes using the `-s` option:

1. Small packet test (100 bytes):
   ```bash
   ./build/udp_toolkit_client -s 100
   ```

2. Medium packet test (1000 bytes):
   ```bash
   ./build/udp_toolkit_client -s 1000
   ```

3. Large packet test (8000 bytes):
   ```bash
   ./build/udp_toolkit_client -s 8000
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

### Log Analysis

After running tests, analyze the logs for performance metrics:

```bash
python3 parse_logs.py --log-file server_debug_YYYYMMDD_HHMMSS.log
```

The log analyzer will:
1. Parse and calculate packet loss statistics
2. Analyze latency metrics (mean, variance, min, max)
3. Calculate throughput statistics
4. Generate visualization graphs

### Test Results Analysis

Collect and analyze:
1. Network latency (average, minimum, maximum, variance)
2. Throughput (average, peak, per-second variation)
3. Packet loss statistics
4. Performance comparison under different network conditions
5. Visual representations of data (histograms and graphs)

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

## Log Analysis Examples

### Packet Loss Analysis
```
Packet Loss Analysis:
Total packets: 10000
Received packets: 9985
Packet loss rate: 0.15%

Sequence Discontinuities (Packet Loss):
  Between sequence 2501 and 2505, lost 3 packets: [2502, 2503, 2504]
  Between sequence 5241 and 5243, lost 1 packets: [5242]
```

### Latency Analysis
```
Latency Analysis:
Mean latency: 12.457689 ms
Latency variance: 3.852146 ms²
Minimum latency: 8.123456 ms
Maximum latency: 25.987654 ms
```

### Throughput Analysis
```
Throughput Analysis (packet size: 1000 Bytes):
Overall average throughput: 4.95 Mbps
Maximum throughput: 5.12 Mbps (at second 15)
Minimum throughput: 4.76 Mbps (at second 3)
``` 