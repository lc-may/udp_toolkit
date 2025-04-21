#define _POSIX_C_SOURCE 200112L  // 为了支持CLOCK_MONOTONIC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>           // clock_gettime, CLOCK_MONOTONIC
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>         // 添加getopt头文件以确保optarg被定义

#define DEFAULT_SERVER_IP "127.0.0.1"
#define SYNC_PORT   4000
#define DATA_PORT   5000
#define DEFAULT_PACKET_SIZE 1000      // bytes
#define DEFAULT_BANDWIDTH   1000000   // bps (1 Mbps)
#define DEFAULT_DURATION    10        // seconds

// 获取单调时钟的浮点秒
static double monotonic_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);                    // 高精度单调时钟
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// 使用NTP算法进行时钟同步：返回client->server的时钟offset（秒）
double sync_clock_ntp(int sock, const char* server_ip) {
    struct sockaddr_in serv = {0};
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(SYNC_PORT);
    inet_pton(AF_INET, server_ip, &serv.sin_addr);

    // 结构体存储所有时间戳
    struct {
        double t1;  // 客户端发送时间
        double t2;  // 服务器接收时间
        double t3;  // 服务器发送时间
    } msg;

    // 记录并发送t1
    msg.t1 = monotonic_sec();
    sendto(sock, &msg, sizeof(msg), 0, 
           (struct sockaddr*)&serv, sizeof(serv));

    // 接收服务器填充的t2和t3
    recvfrom(sock, &msg, sizeof(msg), 0, NULL, NULL);

    // 记录t4
    double t4 = monotonic_sec();

    // 计算往返延迟
    double delay = (t4 - msg.t1) - (msg.t3 - msg.t2);
    
    // 计算时钟偏移
    double offset = ((msg.t2 - msg.t1) + (msg.t3 - t4)) / 2.0;

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
                if (packet_size <= 20) {  // 设置最小包大小，至少需要容纳头部信息
                    fprintf(stderr, "Error: Packet size must be at least 21 bytes\n");
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
    int sock_sync = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_sync < 0) { perror("sock_sync"); return 1; }

    // 2. 计算时钟偏移
    double offset = sync_clock_ntp(sock_sync, server_ip);
    printf("Clock Offset: %.9f seconds\n", offset);
    close(sock_sync);

    // 3. 创建数据发送 socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serv = {0};
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(DATA_PORT);
    inet_pton(AF_INET, server_ip, &serv.sin_addr);

    // 4. 预计算发送间隔
    double interval = (packet_size * 8.0) / bandwidth;      // 秒/包

    // 5. 发送循环 - 基于时间而不是固定包数
    double start_time = monotonic_sec();
    double end_time = start_time + duration;
    int seq = 0;
    
    printf("Starting to send packets to %s, interval %.3f seconds, press Ctrl+C to terminate...\n", server_ip, interval);
    
    while (monotonic_sec() < end_time) {
        double send_ts = monotonic_sec();                    // 取发送时刻

        // 构造 payload：| seq(4B) | send_ts(8B) | offset(8B) | ...
        char* buf = malloc(packet_size);
        if (!buf) {
            perror("malloc");
            break;
        }
        
        memcpy(buf, &seq,      sizeof(seq));
        memcpy(buf+4, &send_ts, sizeof(send_ts));
        memcpy(buf+12, &offset, sizeof(offset));

        sendto(sock, buf, packet_size, 0,
               (struct sockaddr*)&serv, sizeof(serv));
               
        free(buf);

        // 每1000个包输出一次状态
        if (seq % 1000 == 0) {
            printf("Sent %d packets, remaining time %.1f seconds\n", 
                   seq, end_time - monotonic_sec());
        }
        
        seq++;

        // 精确等待下一个间隔
        struct timespec req = {
            .tv_sec  = (time_t)interval,
            .tv_nsec = (long)((interval - (time_t)interval) * 1e9)
        };
        nanosleep(&req, NULL);                              // 精确睡眠
    }

    printf("Test completed! Total packets sent: %d\n", seq);
    close(sock);
    return 0;
}
