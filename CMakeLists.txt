cmake_minimum_required(VERSION 3.10)
project(udp_toolkit C)

# 设置C标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 添加包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 检查并添加所需的库和编译选项
include(CheckSymbolExists)
check_symbol_exists(CLOCK_MONOTONIC "time.h" HAVE_CLOCK_MONOTONIC)

if(NOT HAVE_CLOCK_MONOTONIC)
    add_definitions(-D_POSIX_C_SOURCE=200112L)
endif()

# 创建服务器目标
add_executable(udp_toolkit_server udp_toolkit_server.c)
target_link_libraries(udp_toolkit_server m)  # 链接数学库，用于fabs函数

# 创建客户端目标
add_executable(udp_toolkit_client udp_toolkit_client.c)

# 添加RT库，支持时钟函数
find_library(RT_LIBRARY rt)
if(RT_LIBRARY)
    target_link_libraries(udp_toolkit_server ${RT_LIBRARY})
    target_link_libraries(udp_toolkit_client ${RT_LIBRARY})
endif()

# 安装目标
install(TARGETS udp_toolkit_server udp_toolkit_client
        RUNTIME DESTINATION bin)