#!/bin/bash

# 设置日志文件名（使用当前日期和时间）
LOG_FILE="server_debug_$(date +%Y%m%d_%H%M%S).log"

echo "启动 UDP 工具包服务器，调试输出将被保存到: $LOG_FILE"
echo "按 Ctrl+C 停止服务器"

# 运行服务器，将标准错误输出重定向到日志文件
./build/udp_toolkit_server 2> "$LOG_FILE" 