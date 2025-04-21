#!/bin/bash
#
# Script to run UDP Toolkit client with specific parameters
#

# Run the UDP client with parameters:
# - Server IP: 154.12.95.49
# - Bandwidth: 5000000 bps (5 Mbps)
# - Test duration: 30 seconds
# - Packet size: 1000 bytes

echo "Starting UDP test to 154.12.95.49 with 5Mbps bandwidth for 30 seconds..."

# Execute the client
./build/udp_toolkit_client -i 154.12.95.49 -b 5000000 -t 30 -s 1000

echo "Test completed." 