#!/bin/bash
#
# Script to run UDP Toolkit client with specific parameters
#

# Default values
SERVER_IP="127.0.0.1"
BANDWIDTH=2000000   # 2 Mbps
DURATION=30         # seconds
PACKET_SIZE=1000    # bytes

# Display usage information
function show_usage {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -i IP_ADDRESS  Server IP address (default: $SERVER_IP)"
    echo "  -b BANDWIDTH   Bandwidth in bps (default: $BANDWIDTH)"
    echo "  -t DURATION    Test duration in seconds (default: $DURATION)"
    echo "  -s SIZE        Packet size in bytes (default: $PACKET_SIZE)"
    echo "  -h             Display this help message"
    echo
    echo "Example:"
    echo "  $0 -s 500      # Run test with 500-byte packets"
    echo "  $0 -i 192.168.1.100 -b 5000000 -t 60 -s 1500"
}

# Parse command line arguments
while getopts "i:b:t:s:h" opt; do
    case $opt in
        i) SERVER_IP="$OPTARG" ;;
        b) BANDWIDTH="$OPTARG" ;;
        t) DURATION="$OPTARG" ;;
        s) PACKET_SIZE="$OPTARG" ;;
        h) show_usage; exit 0 ;;
        *) show_usage; exit 1 ;;
    esac
done

echo "Starting UDP test with the following parameters:"
echo "  Server IP:   $SERVER_IP"
echo "  Bandwidth:   $BANDWIDTH bps ($(echo "scale=2; $BANDWIDTH/1000000" | bc) Mbps)"
echo "  Duration:    $DURATION seconds"
echo "  Packet size: $PACKET_SIZE bytes"
echo

# Check if client exists
if [ ! -f "./build/udp_toolkit_client" ]; then
    echo "Error: UDP toolkit client not found in ./build/"
    echo "Please build the project first (cmake . && make)"
    exit 1
fi

# Execute the client
./build/udp_toolkit_client -i "$SERVER_IP" -b "$BANDWIDTH" -t "$DURATION" -s "$PACKET_SIZE"

echo "Test completed." 