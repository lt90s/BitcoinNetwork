#include "addrseed.h"
#include "network.h"
#include "message.h"
#include "reporter.h"

#include <bitcoin/serialize.h>
#include <bitcoin/stream.h>
#include <bitcoin/protocol.h>

#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <fcntl.h>
#include <vector>
#include <iostream>


extern ReporterInterface *gReporter;
static const int DRAIN_SEED_SIZE_PER_LOOP = 128;


bool Connection::sendBuffer(bool &moreWrite)
{
    while (!sendingBuffer.empty()) {
        std::vector<unsigned char> &buffer = sendingBuffer.front();
        int bytesToSend = buffer.size() - sendPos;
        if (bytesToSend <= 0) {
            sendPos = 0;
            sendingBuffer.pop_front();
            continue;
        }

        int wsize = write(sock, &buffer[0] + sendPos, bytesToSend);
        if (wsize < 0) {
            if (errno != EWOULDBLOCK || errno != EINPROGRESS || errno != EINTR) {
                printf("send buffer error: %s\n", strerror(errno));
                return false;
            }
            break;
        } else {
            sendPos += wsize;
            if (wsize != bytesToSend) {
                // partial write
                break;
            } else {
                sendPos = 0;
                sendingBuffer.pop_front();
            }
        }
    }
    moreWrite = !sendingBuffer.empty();
    return true;
}

void Connection::processMessage(uint32_t version, CMessageHeader &header, const std::vector<unsigned char> &buffer, size_t offset)
{
    std::string command(header.command);
    CVectorReader tmpVreader(false, SER_NETWORK, 0, buffer, offset+MESSAGE_HEADER_SIZE);
    if (command == "version") {
        tmpVreader >> youVersion >> youServices;
        CVectorReader vreader(false, SER_NETWORK, youVersion, buffer, offset+MESSAGE_HEADER_SIZE);
        CVersionPayload payload;
        vreader >> payload;
        if (gReporter != nullptr) {
            gReporter->reportVersionedAddr(addrYou.ToStringIP(), addrYou.GetPort(), payload.user_agent, payload.version, payload.services);
        }
        printf("version command: addrme=%s, addryou=%s, agent=%s, version=%d, services=%lu\n",
            payload.addrMe.ToString().c_str(), addrYou.ToString().c_str(), payload.user_agent.c_str(),
            payload.version, payload.services);
        pushVerackCommand();
    } else if (command == "ping") {
        uint64_t nonce;
        tmpVreader >> nonce;
        pushPongCommand(nonce);
    } else if (command == "addr") {
        CVectorReader vreader(true, SER_NETWORK, version, buffer, offset+MESSAGE_HEADER_SIZE);
        uint32_t count = ReadVarInt<CVectorReader, VarIntMode::DEFAULT, uint32_t>(vreader);
        CAddress addr;
        for (auto i = 0; i < count; ++i) {
            // assume valid
            vreader >> addr;
            printf("got new address from %s: %s\n", addrYou.ToString().c_str(), addr.ToString().c_str());
            CAddrSeed::getInstance().addNewAddr(addr);
        }

    }
}

bool Connection::readBuffer(uint32_t version)
{
    unsigned char tmpBuffer[16 * 1024];
    int nread = read(sock, tmpBuffer, sizeof(tmpBuffer));
    if (nread == 0) {
        // peer close
        printf("peer %s closed connection\n", addrYou.ToString().c_str());
        return false;
    } else if (nread < 0) {
        if (errno != EWOULDBLOCK || errno != EINPROGRESS || errno != EINTR) {
            printf("read error %d:%s\n", errno, strerror(errno));
            return false;
        }
        return true;
    }
    
    vReadBuffer.insert(vReadBuffer.end(), tmpBuffer, tmpBuffer + nread);
    size_t offset = 0;
    CVectorReader vreader(false, SER_NETWORK, version, vReadBuffer, 0);
    while (vReadBuffer.size() >= offset + MESSAGE_HEADER_SIZE) {
       
        if (!headerValid) {
            headerValid = true;
            vreader >> header;
            if (header.magic != MAIN_MAGIC) {
                // printf("Invalid magic in header from %s: %#x\n", addrYou.ToString().c_str(), header.magic);
                return false;
            }
        } else {
            vreader.skip(MESSAGE_HEADER_SIZE);
        }
        
        if (vReadBuffer.size() < header.payloadLength + MESSAGE_HEADER_SIZE + offset) {
            break;
        }
        processMessage(version, header, vReadBuffer, offset);
        headerValid = false;
        offset += header.payloadLength + MESSAGE_HEADER_SIZE;
        vreader.skip(header.payloadLength);
    }
    if (offset > 0) {
        size_t leftSize = vReadBuffer.size() - offset;
        memcpy(&vReadBuffer[0], &vReadBuffer[offset], leftSize);
        vReadBuffer.resize(leftSize);
    }
    return true;
}

bool Connection::pushCommand()
{
    sendingBuffer.push_back(std::move(vSendBuffer));
    return true;
}

bool Connection::pushVersionCommand(uint32_t version)
{
    vSendBuffer.clear();
    vSendBuffer.resize(MESSAGE_HEADER_SIZE);
    CVersionPayload versionPayload;
    versionPayload.version = version;
    versionPayload.services = static_cast<uint64_t>(NODE_NETWORK);
    versionPayload.timestamp = time(nullptr);
    versionPayload.addrMe = CAddress(addrMe, version, versionPayload.timestamp);
    versionPayload.addrYou = CAddress(addrYou, version, versionPayload.timestamp);
    versionPayload.nonce = versionPayload.timestamp;
    versionPayload.user_agent = "/Satoshi:0.14.2/";
    versionPayload.start_height = 0;
    versionPayload.relay = false;
    CVectorWriter{false, SER_NETWORK, version, vSendBuffer, MESSAGE_HEADER_SIZE, versionPayload};
    unsigned char hash[32];
    dsha256(&vSendBuffer[MESSAGE_HEADER_SIZE], vSendBuffer.size() - MESSAGE_HEADER_SIZE, hash);
    CMessageHeader hdr(MAIN_MAGIC, "version", vSendBuffer.size() - MESSAGE_HEADER_SIZE, hash);
    CVectorWriter{false, SER_NETWORK, version, vSendBuffer, 0, hdr};
    
    return pushCommand();
}

bool Connection::pushPongCommand(uint64_t nonce)
{
    vSendBuffer.resize(0);
    CVectorWriter{false, SER_NETWORK, 0, vSendBuffer, MESSAGE_HEADER_SIZE, nonce};
    unsigned char hash[32];
    dsha256(&vSendBuffer[MESSAGE_HEADER_SIZE], vSendBuffer.size() - MESSAGE_HEADER_SIZE, hash);
    CMessageHeader hdr(MAIN_MAGIC, "pong", vSendBuffer.size() - MESSAGE_HEADER_SIZE, hash);
    CVectorWriter{false, SER_NETWORK, 0, vSendBuffer, 0, hdr};
    return pushCommand();
}

bool Connection::initializeAddress()
{
    assert(sock > 0);
    struct sockaddr sa;
    memset(&sa, 0, sizeof(sa));
    socklen_t slen = sizeof(sa);
    // ignore error
    getsockname(sock, &sa, &slen);
    addrMe = CService(&sa);
    memset(&sa, 0, sizeof(sa));
    getpeername(sock, &sa, &slen);
    addrYou = CService(&sa);
    return true;
}

bool Connection::pushVerackCommand()
{
    vSendBuffer.clear();
    unsigned char hash[32];
    const unsigned char p[] = "";
    dsha256(p, 0, hash);
    CMessageHeader hdr(MAIN_MAGIC, "verack", 0, hash);
    CVectorWriter{false, SER_NETWORK, 0, vSendBuffer, 0, hdr};
    
    return pushCommand();
}

NetworkCallback *ConnectionManager::initiateConnection(const CService &saddr, int &sock)
{
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    printf("initiate connection to %s\n", saddr.ToString().c_str());
    saddr.GetSockAddr(&addr, &addrlen);
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return nullptr;
    }

    Connection &con = *addConnection(saddr, sock);

    int flag = fcntl(sock, F_GETFL, 0);
	if ( -1 == flag ) {
        close(sock);
        sock = -1;
		return nullptr;
	}
    fcntl(sock, F_SETFL, flag | O_NONBLOCK);
    
    int on = 1;
    int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    if (-1 == ret) {
        close(sock);
        sock = -1;
        return nullptr;
    }
    ret = connect(sock, &addr, sizeof(addr));
    if (ret == -1) {
        int err = errno;
        if (err == EINPROGRESS || err == EWOULDBLOCK) {
            // connecting
            con.status = CONNECTING;
        } else {
            close(sock);
            sock = -1;
            return nullptr;
        }
    } else {
        con.initializeAddress();
        con.status = CONNECTED;
    }
    auto cb = std::bind(&ConnectionManager::networkCallback, this, sock,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    callbacks[sock] = cb;
    qSocks.push_back(sock);
    return &callbacks[sock];
}

Connection *ConnectionManager::addConnection(const CService &addr, int sock)
{
    if (sock < 0) {
        return nullptr;
    }
    Connection con(sock, addr);
    connections[sock] = con;
    return &connections[sock];
}

int ConnectionManager::evictSock()
{
    if (!qSocks.empty()) {
        int sock = qSocks.front();
        qSocks.pop_front();
        return sock;
    }
    return -1;
}

void ConnectionManager::closeConnection(int sock)
{
    if (connections.count(sock) <= 0) {
        return;
    }
    connections.erase(sock);
    callbacks.erase(sock);
    close(sock);
}

bool ConnectionManager::networkCallback(int sock, const struct event &event, int &rsock, bool &moreWrite)
{
    Connection &conn = connections[sock];
    rsock = sock;
    if (event.write) {
        if (conn.status < CONNECTED) {
            printf("connection to %s success\n", conn.addrYou.ToString().c_str());
            conn.initializeAddress();
            conn.status = VERSION_SENT;
            conn.pushVersionCommand(mVersion);
        } else {
            if (conn.status == CONNECTED) {
                conn.pushVersionCommand(mVersion);
                conn.status = VERSION_SENT;
            }
        }
        return conn.sendBuffer(moreWrite);
    } else if (event.read) {
        int ret = conn.readBuffer(conn.youVersion);
        moreWrite = !conn.sendingBuffer.empty();
        return ret;
    } else if (event.error) {
        return false;
    }
    return false;

}

bool NetworkEngine::initEngine()
{
    sp = sp_create(1024);
    if (sp == -1) {
        return false;
    }
    int flag = fcntl(sp, F_GETFL, 0);
	if ( -1 == flag ) {
        close(sp);
        sp = -1;
		return false;
	}
    fcntl(sp, F_SETFL, flag | O_NONBLOCK);
    
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) < 0) {
        printf("getrlimit error");
        return false;
    }
    maxConnections = limit.rlim_cur - 10;
    return true;
}

void NetworkEngine::startEngine()
{
    std::vector<CService *> addrs;
    addrs.reserve(DRAIN_SEED_SIZE_PER_LOOP);
    size_t newSize;
    while (true) {
        newSize = DRAIN_SEED_SIZE_PER_LOOP;
        addrs.resize(0);
        CAddrSeed::getInstance().getNewAddrs(addrs, newSize, false);
        for (auto paddr: addrs) {
            int sock = -1;
            if (connMan.connectionCount() >= maxConnections) {
                int esock = connMan.evictSock();
                if (esock > 0) {
                    sp_del(sp, esock);
                    nPendingEvents--;
                    connMan.closeConnection(esock);
                }
            }
            auto pCallback = connMan.initiateConnection(*paddr, sock);
            if (sock < 0) {
                printf("initiate connection to %s failed: %s\n", paddr->ToString().c_str(), strerror(errno));
                continue;
            }
            int ret = sp_add(sp, sock, reinterpret_cast<void *>(pCallback));
            if (ret < 0) {
                printf("sp_add_write error: %s\n", strerror(errno));
                sp_del(sp, sock);
                nPendingEvents--;
                connMan.closeConnection(sock);
                continue;
            }
           
            writeEnabled[sock] = true;
        }
        nPendingEvents += newSize;
        if (nPendingEvents > events.size()) {
            events.resize(nPendingEvents);        }
        int nActiveEvents = sp_wait(sp, &events[0], nPendingEvents, nullptr);
        dispatchNetworkEvents(nActiveEvents);
    }
}

void NetworkEngine::dispatchNetworkEvents(int nActiveEvents)
{
    std::vector<int> closeSocks;
    for (auto i = 0; i < nActiveEvents; ++i) {
        const struct event &event = events[i];
        NetworkCallback *callback = reinterpret_cast<NetworkCallback *>(event.ud);
        int sock;
        bool moreWrite;
        if(!(*callback)(event, sock, moreWrite)) {
            sp_del(sp, sock);
            closeSocks.push_back(sock);
            continue;
        }
        if (moreWrite) {
            if (!writeEnabled[sock]) {
                int ret = sp_enable_write(sp, sock, reinterpret_cast<void *>(connMan.getNetworkCallback(sock)));
                if (ret == -1) {
                    printf("sp_enable_write sock=%d failed: %s\n", sock, strerror(errno));
                }
                writeEnabled[sock] = true;
            }
        } else {
            if (writeEnabled[sock]) {
                int ret = sp_disable_write(sp, sock, reinterpret_cast<void *>(connMan.getNetworkCallback(sock)));
                if (ret == -1) {
                    printf("sp_disable_write sock=%d failed\n", sock);
                }
                writeEnabled[sock] = false;
            }
        }
    }
    for (auto sock: closeSocks) {
        connMan.closeConnection(sock);
    }
}