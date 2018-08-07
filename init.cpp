#include <bitcoin/protocol.h>

#include "init.h"
#include "addrseed.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <vector>
#include <arpa/inet.h>

bool lookup(const char *hostname, std::vector<CService> &addrs, uint16_t port=8333)
{
    struct addrinfo hint;
    bzero(&hint, sizeof(hint));
    hint.ai_family = PF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = IPPROTO_TCP;
    hint.ai_flags = AI_ADDRCONFIG;

    struct addrinfo *results;
    int error = getaddrinfo(hostname, nullptr, &hint, &results);
    if (error) {
        printf("getaddrinfo for %s failed: %s\n", hostname, gai_strerror(error));
        return false;
    }

    for (auto result = results; result != nullptr; result = result->ai_next) {
        if (result->ai_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)result->ai_addr;
            addrs.push_back(CService(sin->sin_addr, port));
        } else if (result->ai_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)result->ai_addr;
            addrs.push_back(CService(sin6->sin6_addr, port));
        }
    }
    freeaddrinfo(results);
    return true;
}


void initDNSSeedAddr(const std::vector<std::string> &seedNodes)
{    
    std::vector<CService> addrs;
    for (auto node: seedNodes) {
        lookup(node.c_str(), addrs);
    }
    // struct in_addr inaddr;
    // inet_pton(AF_INET, "192.168.0.5", &inaddr);
    // addrs.push_back(CService(CNetAddr(inaddr), 8333));

    for (auto addr: addrs) {
        CAddrSeed::getInstance().addNewAddr(addr);
    }
    
}