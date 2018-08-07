#ifndef __BITCOIN_PROTOCOL_H__
#define __BITCOIN_PROTOCOL_H__

#include <bitcoin/serialize.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/sha.h>

//! nTime field added to CAddress, starting with this version;
//! if possible, avoid requesting addresses nodes older than this
static const int CADDR_TIME_VERSION = 31402;

/** nServices flags */
enum ServiceFlags : uint64_t {
    // Nothing
    NODE_NONE = 0,
    // NODE_NETWORK means that the node is capable of serving the complete block chain. It is currently
    // set by all Bitcoin Core non pruned nodes, and is unset by SPV clients or other light clients.
    NODE_NETWORK = (1 << 0),
    // NODE_GETUTXO means the node is capable of responding to the getutxo protocol request.
    // Bitcoin Core does not support this but a patch set called Bitcoin XT does.
    // See BIP 64 for details on how this is implemented.
    NODE_GETUTXO = (1 << 1),
    // NODE_BLOOM means the node is capable and willing to handle bloom-filtered connections.
    // Bitcoin Core nodes used to support this by default, without advertising this bit,
    // but no longer do as of protocol version 70011 (= NO_BLOOM_VERSION)
    NODE_BLOOM = (1 << 2),
    // NODE_WITNESS indicates that a node can be asked for blocks and transactions including
    // witness data.
    NODE_WITNESS = (1 << 3),
    // NODE_XTHIN means the node supports Xtreme Thinblocks
    // If this is turned off then the node will not service nor make xthin requests
    NODE_XTHIN = (1 << 4),
    // NODE_NETWORK_LIMITED means the same as NODE_NETWORK with the limitation of only
    // serving the last 288 (2 day) blocks
    // See BIP159 for details on how this is implemented.
    NODE_NETWORK_LIMITED = (1 << 10),

    // Bits 24-31 are reserved for temporary experiments. Just pick a bit that
    // isn't getting used, or one not being used much, and notify the
    // bitcoin-development mailing list. Remember that service bits are just
    // unauthenticated advertisements, so your code must be robust against
    // collisions and other cases where nodes may be advertising a service they
    // do not actually support. Other service bits should be allocated via the
    // BIP process.
};

enum Network
{
    NET_UNROUTABLE = 0,
    NET_IPV4,
    NET_IPV6,

    NET_MAX,
};

static const unsigned char pchIPv4[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };

/** IP address (IPv6, or IPv4 using mapped IPv6 range (::FFFF:0:0/96)) */
class CNetAddr
{
    protected:
        unsigned char ip[16]; // in network byte order
        uint32_t scopeId; // for scoped/link-local ipv6 addresses

    public:
        CNetAddr() {
            memset(ip, 0, sizeof(ip));
            scopeId = 0;
        }
        explicit CNetAddr(const struct in_addr& ipv4Addr) {
            SetRaw(NET_IPV4, (const uint8_t*)&ipv4Addr);
        }
        void SetIP(const CNetAddr& ipIn) {
            memcpy(ip, ipIn.ip, sizeof(ip));
        }

    private:
        /**
         * Set raw IPv4 or IPv6 address (in network byte order)
         * @note Only NET_IPV4 and NET_IPV6 are allowed for network.
         */
        void SetRaw(Network network, const uint8_t *ip_in) {
            switch(network)
            {
                case NET_IPV4:
                    memcpy(ip, pchIPv4, 12);
                    memcpy(ip+12, ip_in, 4);
                    break;
                case NET_IPV6:
                    memcpy(ip, ip_in, 16);
                    break;
                default:
                    assert(!"invalid network");
            }
        }
    public:
        // IPv4 mapped address (::FFFF:0:0/96, 0.0.0.0/0)
        bool IsIPv4() const {
            return (memcmp(ip, pchIPv4, sizeof(pchIPv4)) == 0);
        }
        bool IsIPv6() const {
            return !IsIPv4();
        }
        bool IsLocal() const {
            // IPv4 loopback
            if (IsIPv4() && (GetByte(3) == 127 || GetByte(3) == 0))
                return true;

            // IPv6 loopback (::1/128)
            static const unsigned char pchLocal[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
            if (memcmp(ip, pchLocal, 16) == 0)
                return true;

            return false;
        }
        std::string ToString() const {
            return ToStringIP();
        }
        std::string ToStringIP() const {
            char buf[64];
            if (IsIPv4()) {
                sprintf(buf, "%u.%u.%u.%u", GetByte(3), GetByte(2), GetByte(1), GetByte(0));
            } else {
                sprintf(buf, "%x:%x:%x:%x:%x:%x:%x:%x",
                        GetByte(15) << 8 | GetByte(14), GetByte(13) << 8 | GetByte(12),
                        GetByte(11) << 8 | GetByte(10), GetByte(9) << 8 | GetByte(8),
                        GetByte(7) << 8 | GetByte(6), GetByte(5) << 8 | GetByte(4),
                        GetByte(3) << 8 | GetByte(2), GetByte(1) << 8 | GetByte(0));
            }
            return buf;
        }
        unsigned int GetByte(int n) const {
            return ip[15-n];
        }
        uint64_t GetHash() const {
            return 0;
        }
        bool GetInAddr(struct in_addr* pipv4Addr) const {
            if (!IsIPv4())
                return false;
            memcpy(pipv4Addr, ip+12, 4);
            return true;
        }

        explicit CNetAddr(const struct in6_addr& pipv6Addr, const uint32_t scope = 0) {
            SetRaw(NET_IPV6, (const uint8_t*)&pipv6Addr);
            scopeId = scope;
        }
        bool GetIn6Addr(struct in6_addr* pipv6Addr) const {
            memcpy(pipv6Addr, ip, 16);
            return true;
        }

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(ip);
        }

        friend bool operator<(const CNetAddr& a, const CNetAddr& b);
};

inline bool operator<(const CNetAddr& a, const CNetAddr& b)
{
    return (memcmp(a.ip, b.ip, 16) < 0);
}

/** A combination of a network address (CNetAddr) and a (TCP) port */
class CService : public CNetAddr
{
    protected:
        uint16_t port; // host order

    public:
        CService(): CNetAddr(), port(0) {}
        CService(const CNetAddr& ip, unsigned short port): CNetAddr(ip), port(port) {}
        CService(const struct in_addr& ipv4Addr, unsigned short port): CNetAddr(ipv4Addr), port(port) {}
        explicit CService(const struct sockaddr_in& addr): CNetAddr(addr.sin_addr), port(ntohs(addr.sin_port)) {};
        explicit CService(const struct sockaddr_in6& addr): CNetAddr(addr.sin6_addr), port(ntohs(addr.sin6_port)) {};
        CService(const struct in6_addr& ipv6Addr, unsigned short port): CNetAddr(ipv6Addr), port(port) {}
        explicit CService(const struct sockaddr *addr) {
            SetSockAddr(addr);
        }
        unsigned short GetPort() const {
            return port;
        }
        bool GetSockAddr(struct sockaddr* paddr, socklen_t *addrlen) const {
            if (IsIPv4()) {
                if (*addrlen < (socklen_t)sizeof(struct sockaddr_in))
                    return false;
                *addrlen = sizeof(struct sockaddr_in);
                struct sockaddr_in *paddrin = (struct sockaddr_in*)paddr;
                memset(paddrin, 0, *addrlen);
                if (!GetInAddr(&paddrin->sin_addr)) {
                    return false;
                }
                paddrin->sin_family = AF_INET;
                paddrin->sin_port = htons(port);
                return true;
            } else {
                return false;
            }
        }
        bool SetSockAddr(const struct sockaddr* paddr) {
            switch (paddr->sa_family) {
                case AF_INET:
                    *this = CService(*(const struct sockaddr_in*)paddr);
                    return true;
                case AF_INET6:
                *this = CService(*(const struct sockaddr_in6*)paddr);
                default:
                    return false;
            }
        }
        //std::vector<unsigned char> GetKey() const;
        std::string ToString() const {
            return ToStringIPPort();
        }
        std::string ToStringPort() const {
            char buf[8];
            sprintf(buf, "%u", port);
            return buf;
        }
        std::string ToStringIPPort() const {
            if (IsIPv4()) {
                return ToStringIP() + ":" + ToStringPort();
            } else {
                return "[" + ToStringIP() + "]:" + ToStringPort();
            }
        }

        // CService(const struct in6_addr& ipv6Addr, unsigned short port);
        // explicit CService(const struct sockaddr_in6& addr);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(ip);
            READWRITE(WrapBigEndian(port));
        }
};


/** A CService with information about it as peer */
class CAddress : public CService
{
public:
    CAddress(): CService() {
        Init();
    }
    explicit CAddress(CService ipIn, uint64_t nServicesIn, unsigned int nTimeIn): CService(ipIn) {
        nServices = static_cast<ServiceFlags>(nServicesIn);
        nTime = nTimeIn;
    }

    void Init() {
        nServices = NODE_NONE;
        nTime = 100000000;
    }


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (ser_action.ForRead())
            Init();
        int nVersion = s.GetVersion();
        if (s.GetType() & SER_DISK)
            READWRITE(nVersion);
        if (s.HaveTime() && ((s.GetType() & SER_DISK) ||
            (nVersion >= CADDR_TIME_VERSION && !(s.GetType() & SER_GETHASH))))
            READWRITE(nTime);
        uint64_t nServicesInt = nServices;
        READWRITE(nServicesInt);
        nServices = static_cast<ServiceFlags>(nServicesInt);
        READWRITEAS(CService, *this);
    }

    // TODO: make private (improves encapsulation)
public:
    ServiceFlags nServices;
    
    // disk and network only
    unsigned int nTime;
};

static void dsha256(const unsigned char *s, size_t slen, unsigned char hash[32])
{
    unsigned char tmp[32];
    SHA256(s, slen, tmp);
    SHA256(tmp, 32, hash);
}
#endif
