#!/bin/bash
#
# Script to run UDP Toolkit client with specific parameters
#

# Run the UDP client with parameters:
# - Server IP: 154.12.95.49
# - Bandwidth: 2000000 bps (2 Mbps)
# - Test duration: 30 seconds
# - Packet size: 1000 bytes

echo "Starting UDP test to 127.0.0.1 with 2Mbps bandwidth for 30 seconds..."

# Execute the client
./build/udp_toolkit_client -i 127.0.0.1 -b 2000000 -t 30 -s 1000

echo "Test completed." 