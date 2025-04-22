#define _POSIX_C_SOURCE 200112L  // 为了支持CLOCK_MONOTONIC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>           // clock_gettime, CLOCK_MONOTONIC
#include <sys/socket.h>
#include <sys/time.h>       // struct timeval, 用于socket超时设置
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>         // 添加getopt头文件以确保optarg被定义
#include <errno.h>          // errno
#include <fcntl.h>          // fcntl, O_NONBLOCK

#define DEFAULT_SERVER_IP "127.0.0.1"
#define SYNC_PORT   4000
#define DATA_PORT   5000
#define DEFAULT_PACKET_SIZE 1000      // bytes
#define DEFAULT_BANDWIDTH   1000000   // bps (1 Mbps)
#define DEFAULT_DURATION    10        // seconds
#define HEADER_SIZE         20        // Seq(4) + send_ts(8) + offset(8) + packet_size(4)

// 获取单调时钟的浮点秒
static double monotonic_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);                    // 高精度单调时钟
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// 使用NTP算法进行时钟同步：返回client->server的时钟offset（秒）
double sync_clock_ntp(int sock, const char* server_ip) {
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    
    // 初始化服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SYNC_PORT);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Error: Invalid IP address format\n");
        return 0.0;
    }

    // 准备NTP样式时间同步消息
    double t1, t2, t3, t4;
    char buffer[sizeof(double) * 3];  // 存储t1,t2,t3
    ssize_t bytes_sent, bytes_received;
    
    // 记录发送时间t1
    t1 = monotonic_sec();
    
    // 准备发送的消息
    memcpy(buffer, &t1, sizeof(t1));
    
    // 发送t1到服务器
    bytes_sent = sendto(sock, buffer, sizeof(double), 0, 
                 (struct sockaddr*)&server_addr, server_addr_len);
    if (bytes_sent < 0) {
        perror("Error sending sync request");
        return 0.0;
    }
    
    // 等待服务器回复(t2,t3)
    bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0,
                    (struct sockaddr*)&server_addr, &server_addr_len);
    if (bytes_received < 0) {
        perror("Error receiving sync response");
        return 0.0;
    }
    
    // 记录接收时间t4
    t4 = monotonic_sec();
    
    // 从回复中提取t2
    memcpy(&t2, buffer, sizeof(t2));
    t3 = t2;  // 在简化的服务器中，t3≈t2
    
    // 计算往返延迟
    double delay = (t4 - t1) - (t3 - t2);
    
    // 计算时钟偏移
    double offset = ((t2 - t1) + (t3 - t4)) / 2.0;
    
    return offset;
}

// 验证IPv4地址格式
int validate_ipv4(const char* ip_str) {
    struct in_addr addr;
    return inet_pton(AF_INET, ip_str, &addr) == 1;
}

// 打印使用帮助
void print_usage(const char* prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -i ip_address   Specify server IP address (default: %s)\n", DEFAULT_SERVER_IP);
    printf("  -b bandwidth    Specify sending bandwidth in bps (default: %d)\n", DEFAULT_BANDWIDTH);
    printf("  -t time         Specify test duration in seconds (default: %d)\n", DEFAULT_DURATION);
    printf("  -s size         Specify packet size in bytes (default: %d)\n", DEFAULT_PACKET_SIZE);
    printf("  -h              Display this help message\n");
    printf("Example:\n");
    printf("  %s -i 192.168.1.100 -b 5000000 -t 30 -s 500    Test with 5Mbps bandwidth for 30 seconds using 500-byte packets\n", prog_name);
}

// 动态计算发送间隔（秒）
double calculate_interval(int packet_size, long bandwidth) {
    // 转换为比特，然后除以带宽（bps）
    return (packet_size * 8.0) / bandwidth;
}

int main(int argc, char* argv[]) {
    // 参数默认值
    long bandwidth = DEFAULT_BANDWIDTH;
    int duration = DEFAULT_DURATION;
    int packet_size = DEFAULT_PACKET_SIZE;
    char server_ip[16] = DEFAULT_SERVER_IP;
    
    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "i:b:t:s:h")) != -1) {
        switch (opt) {
            case 'i':
                if (!validate_ipv4(optarg)) {
                    fprintf(stderr, "Error: Invalid IPv4 address format\n");
                    return 1;
                }
                strncpy(server_ip, optarg, sizeof(server_ip) - 1);
                server_ip[sizeof(server_ip) - 1] = '\0';  // 确保字符串以null结尾
                break;
            case 'b':
                bandwidth = atol(optarg);
                if (bandwidth <= 0) {
                    fprintf(stderr, "Error: Bandwidth must be positive\n");
                    return 1;
                }
                break;
            case 't':
                duration = atoi(optarg);
                if (duration <= 0) {
                    fprintf(stderr, "Error: Test duration must be positive\n");
                    return 1;
                }
                break;
            case 's':
                packet_size = atoi(optarg);
                if (packet_size <= HEADER_SIZE) {  // 确保包大小足够容纳头部
                    fprintf(stderr, "Error: Packet size must be at least %d bytes\n", HEADER_SIZE + 1);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    printf("Configuration: Server IP = %s, Bandwidth = %ld bps, Test Duration = %d seconds, Packet Size = %d bytes\n", 
           server_ip, bandwidth, duration, packet_size);

    // 1. 创建同步 socket
    int sock_sync = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_sync < 0) { 
        perror("Error creating sync socket"); 
        return 1; 
    }
    
    // 设置接收超时
    struct timeval tv;
    tv.tv_sec = 5;  // 5秒超时
    tv.tv_usec = 0;
    if (setsockopt(sock_sync, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting socket timeout");
        close(sock_sync);
        return 1;
    }

    // 2. 计算时钟偏移
    double offset = sync_clock_ntp(sock_sync, server_ip);
    printf("Clock Offset: %.9f seconds\n", offset);
    close(sock_sync);

    // 3. 创建数据发送 socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("Error creating data socket");
        return 1;
    }
    
    // 设置socket为非阻塞模式
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        perror("Error getting socket flags");
        close(sock);
        return 1;
    }
    
    flags |= O_NONBLOCK;
    if (fcntl(sock, F_SETFL, flags) == -1) {
        perror("Error setting socket to non-blocking mode");
        close(sock);
        return 1;
    }
    
    // 设置目标地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DATA_PORT);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Error: Invalid IP address\n");
        close(sock);
        return 1;
    }

    // 4. 初始计算发送间隔
    double initial_interval = calculate_interval(packet_size, bandwidth);
    printf("Initial interval: %.9f seconds (theoretical)\n", initial_interval);

    // 5. 分配包缓冲区（只分配一次）
    char* packet_buffer = (char*)malloc(packet_size);
    if (!packet_buffer) {
        perror("Error allocating packet buffer");
        close(sock);
        return 1;
    }
    
    // 初始化缓冲区（只有头部会被覆盖，其余部分可以预填充）
    memset(packet_buffer, 0, packet_size);

    // 6. 发送循环 - 基于时间而不是固定包数
    double start_time = monotonic_sec();
    double end_time = start_time + duration;
    int seq = 0;
    double next_send_time = start_time;
    int retry_count = 0;
    
    printf("Starting to send packets to %s, press Ctrl+C to terminate...\n", server_ip);
    
    while (monotonic_sec() < end_time) {
        double send_ts = monotonic_sec();
        
        // 可以动态调整单个包的大小（这里示例固定使用命令行参数指定的大小）
        int current_packet_size = packet_size;
        
        // 重新计算此包的发送间隔（如果包大小可变）
        double current_interval = calculate_interval(current_packet_size, bandwidth);
        
        // 构造 payload：| seq(4B) | send_ts(8B) | offset(8B) | packet_size(4B) | ...
        memcpy(packet_buffer, &seq, sizeof(seq));
        memcpy(packet_buffer + 4, &send_ts, sizeof(send_ts));
        memcpy(packet_buffer + 12, &offset, sizeof(offset));
        memcpy(packet_buffer + 20, &current_packet_size, sizeof(current_packet_size));

        // 发送数据包
        ssize_t bytes_sent = sendto(sock, packet_buffer, current_packet_size, 0,
                           (struct sockaddr*)&server_addr, sizeof(server_addr));
        
        if (bytes_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞socket可能出现的暂时错误，可以重试
                retry_count++;
                if (retry_count > 5) {
                    // 如果多次重试仍失败，输出警告并继续
                    printf("Warning: Send buffer full, packet %d dropped after %d retries\n", 
                           seq, retry_count);
                    retry_count = 0;
                    seq++;  // 仍然递增序列号以保持连续性
                }
                // 等待一小段时间再重试
                //usleep(1000);  // 1毫秒
                continue;
            } else {
                perror("Error sending packet");
                break;
            }
        } else {
            retry_count = 0;  // 重置重试计数器
        }

        // 每1000个包输出一次状态
        if (seq % 1000 == 0) {
            printf("Sent %d packets, size=%d bytes, interval=%.9f sec, remaining time %.1f seconds\n", 
                   seq, current_packet_size, current_interval, end_time - monotonic_sec());
        }
        
        seq++;

        // 计算下一个发送时间点
        next_send_time = start_time + (seq * current_interval);
        
        // 计算需要睡眠的时间（精确控制发送速率）
        double current_time = monotonic_sec();
        double sleep_time = next_send_time - current_time;
        
        // 只有需要睡眠时才睡眠
        if (sleep_time > 0) {
            struct timespec req = {
                .tv_sec = (time_t)sleep_time,
                .tv_nsec = (long)((sleep_time - (time_t)sleep_time) * 1e9)
            };
            nanosleep(&req, NULL);
        } else if (sleep_time < -0.1) {
            // 如果严重落后于计划发送时间（超过100ms），输出警告
            printf("Warning: Sending rate too high, behind schedule by %.3f seconds\n", -sleep_time);
        }
    }

    printf("Test completed! Total packets sent: %d\n", seq);
    
    // 释放资源
    free(packet_buffer);
    close(sock);
    return 0;
}
