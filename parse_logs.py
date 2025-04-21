import re
import numpy as np
import matplotlib.pyplot as plt
from collections import Counter
import argparse  # 添加argparse模块用于处理命令行参数

def parse_log_file(file_path):
    # Store latency values, sequence numbers, and send timestamps
    latencies = []
    sequences = []
    send_timestamps = []
    
    # Compile regex to extract data with the new Send_ts field
    seq_pattern = re.compile(r'Seq=(\d+), Send_ts=([\d.]+), Latency=([\d.]+) ms')
    
    with open(file_path, 'r') as file:
        for line in file:
            match = seq_pattern.search(line)
            if match:
                seq = int(match.group(1))
                send_ts = float(match.group(2))
                latency = float(match.group(3))
                sequences.append(seq)
                send_timestamps.append(send_ts)
                latencies.append(latency)
    
    return sequences, send_timestamps, latencies

def analyze_packet_loss(sequences):
    # Cannot analyze without sequence numbers
    if not sequences:
        return 0, [], []
    
    # 排序序列号
    sorted_seq = sorted(sequences)
    
    # 找出不连续的地方
    discontinuities = []
    for i in range(len(sorted_seq) - 1):
        if sorted_seq[i + 1] - sorted_seq[i] > 1:
            gap_start = sorted_seq[i]
            gap_end = sorted_seq[i + 1]
            missing_seq = list(range(gap_start + 1, gap_end))
            discontinuities.append((gap_start, gap_end, missing_seq))
    
    # Expected range of sequence numbers
    expected_range = set(range(min(sequences), max(sequences) + 1))
    actual_set = set(sequences)
    
    # Lost packet sequence numbers
    lost_packets = expected_range - actual_set
    
    # Packet loss rate
    total_expected = len(expected_range)
    loss_rate = len(lost_packets) / total_expected if total_expected > 0 else 0
    
    return loss_rate, sorted(lost_packets), discontinuities

def analyze_latency(latencies):
    if not latencies:
        return None, None, None, None
    
    # Convert to numpy array for calculations
    latency_array = np.array(latencies)
    
    # Calculate statistics
    mean = np.mean(latency_array)
    variance = np.var(latency_array)
    min_latency = np.min(latency_array)
    max_latency = np.max(latency_array)
    
    return mean, variance, min_latency, max_latency

def plot_latency_histogram(latencies, output_file="latency_histogram.png"):
    plt.figure(figsize=(10, 6))
    plt.hist(latencies, bins=30, alpha=0.7, color='blue')
    plt.title('Latency Distribution Histogram')
    plt.xlabel('Latency (ms)')
    plt.ylabel('Frequency')
    plt.grid(True, alpha=0.3)
    plt.savefig(output_file)
    plt.close()

def calculate_throughput(sequences, send_timestamps, packet_size=1000):
    """
    Calculate network throughput based on send timestamps.
    
    Args:
        sequences: List of sequence numbers
        send_timestamps: List of send timestamps
        packet_size: Size of each packet in Bytes
    
    Returns:
        overall_throughput: Average throughput for the entire session in Mbps
        throughput_per_second: Dictionary mapping each second to its throughput in Mbps
    """
    if not sequences or not send_timestamps or len(sequences) != len(send_timestamps):
        return 0, {}
    
    # Sort data by timestamp to ensure chronological order
    data = sorted(zip(sequences, send_timestamps), key=lambda x: x[1])
    seq_sorted = [x[0] for x in data]
    ts_sorted = [x[1] for x in data]
    
    # Overall throughput calculation
    duration = ts_sorted[-1] - ts_sorted[0]  # Total duration in seconds
    total_bits = len(sequences) * packet_size * 8  # Total bits transferred
    overall_throughput = (total_bits / duration) / 1_000_000  # Convert to Mbps
    
    # Per-second throughput calculation
    throughput_per_second = {}
    
    # Get the starting second (floor)
    start_second = int(ts_sorted[0])
    end_second = int(ts_sorted[-1]) + 1
    
    for second in range(start_second, end_second):
        # Find packets sent in this second
        packets_in_second = [
            seq for seq, ts in zip(seq_sorted, ts_sorted) 
            if second <= ts < second + 1
        ]
        
        if packets_in_second:
            # Calculate throughput for this second (Mbps)
            bits_in_second = len(packets_in_second) * packet_size * 8
            throughput_per_second[second] = bits_in_second / 1_000_000
    
    return overall_throughput, throughput_per_second

def plot_throughput(throughput_per_second, output_file="throughput_graph.png"):
    """
    Plot throughput over time.
    
    Args:
        throughput_per_second: Dictionary mapping seconds to throughput in Mbps
        output_file: Path to save the plot
    """
    if not throughput_per_second:
        return
    
    seconds = sorted(throughput_per_second.keys())
    throughputs = [throughput_per_second[s] for s in seconds]
    
    # Normalize time to start from 0
    start_time = min(seconds)
    normalized_seconds = [s - start_time for s in seconds]
    
    plt.figure(figsize=(12, 6))
    plt.plot(normalized_seconds, throughputs, marker='o', linestyle='-', markersize=3)
    plt.title('Network Throughput Over Time')
    plt.xlabel('Time (seconds from start)')
    plt.ylabel('Throughput (Mbps)')
    plt.grid(True, alpha=0.3)
    plt.savefig(output_file)
    plt.close()

def main():
    # 添加命令行参数解析
    parser = argparse.ArgumentParser(description='Analyze UDP network log files')
    parser.add_argument('--log-file', type=str, default="server_debug_20250420_225135.log",
                        help='Path to the log file to analyze')
    parser.add_argument('--packet-size', type=int, default=1000,
                        help='Size of each packet in Bytes (default: 1000)')
    args = parser.parse_args()
    
    log_file = args.log_file
    packet_size = args.packet_size
    
    print(f"Parsing log file: {log_file}")
    sequences, send_timestamps, latencies = parse_log_file(log_file)
    
    if not sequences:
        print("No packet sequence data found")
        return
    
    # Analyze packet loss
    loss_rate, lost_packets, discontinuities = analyze_packet_loss(sequences)
    print(f"\nPacket Loss Analysis:")
    print(f"Total packets: {max(sequences) + 1}")
    print(f"Received packets: {len(sequences)}")
    print(f"Packet loss rate: {loss_rate:.2%}")
    
    # 打印序列不连续的地方
    if discontinuities:
        print(f"\nSequence Discontinuities (Packet Loss):")
        for disc in discontinuities:
            start, end, missing = disc
            print(f"  Between sequence {start} and {end}, lost {len(missing)} packets: {missing}")
    else:
        print(f"\nAll sequences are continuous, no packet loss detected")
    
    if lost_packets:
        print(f"Lost packet sequences: {lost_packets if len(lost_packets) < 20 else str(list(lost_packets)[:20]) + '...'}")
    
    # Analyze latency
    mean, variance, min_latency, max_latency = analyze_latency(latencies)
    print(f"\nLatency Analysis:")
    print(f"Mean latency: {mean:.6f} ms")
    print(f"Latency variance: {variance:.6f} ms²")
    print(f"Minimum latency: {min_latency:.6f} ms")
    print(f"Maximum latency: {max_latency:.6f} ms")
    
    # Calculate and display throughput
    overall_throughput, throughput_per_second = calculate_throughput(
        sequences, send_timestamps, packet_size
    )
    
    print(f"\nThroughput Analysis (packet size: {packet_size} Bytes):")
    print(f"Overall average throughput: {overall_throughput:.2f} Mbps")
    
    if throughput_per_second:
        max_second = max(throughput_per_second.items(), key=lambda x: x[1])
        min_second = min(throughput_per_second.items(), key=lambda x: x[1])
        print(f"Maximum throughput: {max_second[1]:.2f} Mbps (at second {max_second[0]})")
        print(f"Minimum throughput: {min_second[1]:.2f} Mbps (at second {min_second[0]})")
        
        # Generate throughput graph
        plot_throughput(throughput_per_second)
        print(f"\nThroughput graph saved to 'throughput_graph.png'")
    
    # Generate latency histogram
    if latencies:
        plot_latency_histogram(latencies)
        print(f"\nLatency histogram saved to 'latency_histogram.png'")

if __name__ == "__main__":
    main() 