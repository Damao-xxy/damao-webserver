#include <strings.h>
#include <string.h>

#include <InetAddress.h>

/*struct in_addr {
    in_addr_t s_addr;  // 32位IPv4地址（网络字节序）
};

struct sockaddr_in {
    sa_family_t sin_family;  // 地址族（AF_INET）
    in_port_t sin_port;      // 端口号（网络字节序）
    struct in_addr sin_addr; // IPv4地址
    char sin_zero[8];        // 填充字段，使大小与sockaddr一致
};*/
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    ::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = ::htons(port);  // 本地字节序转为网络字节序
    addr_.sin_addr.s_addr = ::inet_addr(ip.c_str());
}

std::string InetAddress::toIp() const
{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);  //inet_ntop将网络字节序的 IP 地址转为点分十进制字符串
    return buf;
}

/*
流程解析：
先转换 IP 为字符串（如 "192.168.1.1"）。
ntohs将网络字节序端口转为本地字节序（如 8080）。
拼接 IP 和端口（如 "192.168.1.1:8080"）。
*/
std::string InetAddress::toIpPort() const
{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = ::strlen(buf);
    uint16_t port = ::ntohs(addr_.sin_port);
    sprintf(buf+end, ":%u", port);
    return buf;
}

uint16_t InetAddress::toPort() const
{
    return ::ntohs(addr_.sin_port);
}

#if 0
#include <iostream>
int main()
{
    InetAddress addr(8080);
    std::cout << addr.toIpPort() << std::endl;
}
#endif