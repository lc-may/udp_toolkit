#define _POSIX_C_SOURCE 200112L  // For CLOCK_MONOTONIC support

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>           // clock_gettime, CLOCK_MONOTONIC
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <math.h>           // fabs
#include <stdint.h>         // uint64_t
#include <arpa/inet.h>      // inet_ntoa
#include <stdarg.h>         // va_list, va_start, va_end

#define SYNC_PORT   4000
#define DATA_PORT   5000
#define PACKET_SIZE 1000    // Keep consistent with client
#define DEBUG       1       // Set to 0 to disable debug output

// Get monotonic clock time in seconds
static double monotonic_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// Debug print function
static void debug_print(const char* format, ...) {
    if (DEBUG) {
        va_list args;
        va_start(args, format);
        fprintf(stderr, "[DEBUG] ");
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

// 服务器端处理时钟同步请求
void handle_time_sync(int sock, struct sockaddr_in* client_addr, socklen_t addr_len) {
    struct {
        double t1;  // 客户端发送时间
        double t2;  // 服务器接收时间
        double t3;  // 服务器发送时间
    } msg;

    // 接收客户端的t1
    recvfrom(sock, &msg, sizeof(msg), 0, 
             (struct sockaddr*)client_addr, &addr_len);

    // 记录t2
    msg.t2 = monotonic_sec();
    
    // 记录t3
    msg.t3 = monotonic_sec();

    // 发送t2和t3回客户端
    sendto(sock, &msg, sizeof(msg), 0,
           (struct sockaddr*)client_addr, addr_len);
}

int main() {
    // --- 1. Initialize Statistics Variables ---
    double start_sec    = monotonic_sec();  // Test start time
    double last_sec     = start_sec;        // Last throughput output time
    uint64_t bytes_interval = 0;            // Current interval bytes
    uint64_t total_bytes    = 0;            // Total received bytes
    uint64_t sync_requests  = 0;            // Clock sync request counter

    printf("UDP Toolkit Server started - Clock Sync Port: %d, Data Port: %d\n", SYNC_PORT, DATA_PORT);
    debug_print("Debug mode enabled\n");

    // --- 2. Create and bind SYNC socket ---
    int sync_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sync_sock < 0) { perror("sync socket"); return 1; }
    struct sockaddr_in sync_addr = {0};
    sync_addr.sin_family      = AF_INET;
    sync_addr.sin_port        = htons(SYNC_PORT);
    sync_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sync_sock, (struct sockaddr*)&sync_addr, sizeof(sync_addr)) < 0) {
        perror("sync bind"); close(sync_sock); return 1;
    }
    debug_print("Clock sync socket bound to port %d\n", SYNC_PORT);

    // --- 3. Create and bind DATA socket ---
    int data_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (data_sock < 0) { perror("data socket"); return 1; }
    struct sockaddr_in data_addr = {0};
    data_addr.sin_family      = AF_INET;
    data_addr.sin_port        = htons(DATA_PORT);
    data_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        perror("data bind"); close(data_sock); return 1;
    }
    debug_print("Data socket bound to port %d\n", DATA_PORT);

    // --- 4. Main loop: select to monitor SYNC and DATA ---
    fd_set readfds;
    int maxfd = (sync_sock > data_sock ? sync_sock : data_sock) + 1;
    debug_print("Server main loop started...\n");
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sync_sock, &readfds);
        FD_SET(data_sock, &readfds);

        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // --- 4.1 Handle clock synchronization requests ---
        if (FD_ISSET(sync_sock, &readfds)) {
            struct sockaddr_in cli;
            socklen_t len = sizeof(cli);
            
            handle_time_sync(sync_sock, &cli, len);
        }

        // --- 4.2 Handle data packet reception and latency calculation ---
        if (FD_ISSET(data_sock, &readfds)) {
            char buf[PACKET_SIZE];
            struct sockaddr_in cli;
            socklen_t len = sizeof(cli);
            ssize_t n = recvfrom(data_sock, buf, PACKET_SIZE, 0,
                                 (struct sockaddr*)&cli, &len);
            // Verify packet contains at least seq + send_ts(8B) + offset(8B)
            if (n >= (ssize_t)(sizeof(int) + sizeof(double)*2)) {
                // --- 4.2.1 Get reception timestamp ---
                double recv_sec = monotonic_sec();

                // --- 4.2.2 Parse seq, send_ts, offset ---
                int    seq;
                double send_ts, offset;
                size_t pos = 0;
                memcpy(&seq,     buf + pos, sizeof(seq));      pos += sizeof(seq);
                memcpy(&send_ts, buf + pos, sizeof(send_ts));  pos += sizeof(send_ts);
                memcpy(&offset,  buf + pos, sizeof(offset));

                // --- 4.2.3 Calculate and print one-way latency (milliseconds) ---
                double latency = recv_sec - (send_ts + offset);
                debug_print("Seq=%d, Send_ts=%.9f, Latency=%.6f ms\n",
                       seq, send_ts, fabs(latency) * 1e3);
                
                if (DEBUG && seq % 1000 == 0) {
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(cli.sin_addr), client_ip, INET_ADDRSTRLEN);
                    debug_print("Packet details (seq=%d):\n", seq);
                    debug_print("  → Source: %s:%d\n", client_ip, ntohs(cli.sin_port));
                    debug_print("  → Send time: %.9f\n", send_ts);
                    debug_print("  → Offset: %.9f\n", offset);
                    debug_print("  → Receive time: %.9f\n", recv_sec);
                    debug_print("  → Size: %zd bytes\n", n);
                }

                // --- 4.2.4 Accumulate byte statistics ---
                bytes_interval += (uint64_t)n;
                total_bytes    += (uint64_t)n;
            } else {
                debug_print("Received invalid data packet (size: %zd, min expected: %zd)\n", 
                           n, sizeof(int) + sizeof(double)*2);
            }
        }

        // // --- 5. Sample throughput every second & calculate average ---
        // {
        //     double now_sec = monotonic_sec();
        //     if (now_sec - last_sec >= 1.0) {
        //         double interval = now_sec - last_sec;           // Real elapsed time
        //         // bps = bits / sec
        //         double sample_tps = (bytes_interval * 8.0) / interval;
        //         double avg_tps    = (total_bytes   * 8.0) / (now_sec - start_sec);

        //         printf("[%.0f-%.0f s] Sample Throughput: %.3f bps, "
        //                "Average Throughput: %.3f bps\n",
        //                last_sec - start_sec,
        //                now_sec  - start_sec,
        //                sample_tps,
        //                avg_tps);
                       
        //         debug_print("Stats update: interval_bytes=%llu, total_bytes=%llu, sync_requests=%llu\n",
        //                    bytes_interval, total_bytes, sync_requests);

        //         // Reset sampling interval
        //         bytes_interval = 0;
        //         last_sec       = now_sec;
        //     }
        // }
    }

    debug_print("Server shutting down...\n");
    close(sync_sock);
    close(data_sock);
    return 0;
}
