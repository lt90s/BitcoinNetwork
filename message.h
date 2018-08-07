#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <bitcoin/serialize.h>
#include <bitcoin/protocol.h>

#include <stdint.h>
#include <string.h>

static const uint32_t MAIN_MAGIC = 0xD9B4BEF9;
static const int MESSAGE_HEADER_SIZE = 24;

class CMessageHeader
{
public:
    CMessageHeader() {
        init();
    }
    CMessageHeader(uint32_t _magic, const char *_command, uint32_t _length, unsigned char _checksum[4]) {
        magic = _magic;
        bzero(command, sizeof(command));
        assert(strlen(_command) <= 12);
        memcpy(command, _command, strlen(_command));
        payloadLength = _length;
        uint32_t __checksum;
        memcpy(&__checksum, _checksum, 4);
        checksum = __checksum;
    }
    uint32_t magic;
    char command[12];
    uint32_t payloadLength;
    uint32_t checksum;

    ADD_SERIALIZE_METHODS

    void init() {
        magic = 0;
        bzero(command, sizeof(command));
        payloadLength = 0;
        checksum = 0;
    }

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead()) {
            init();
        }
        READWRITE(magic);
        READWRITE(command);
        READWRITE(payloadLength);
        READWRITE(checksum);
    }
};

class CVersionPayload
{
public:
    CVersionPayload() {
        init();
    }
    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        if (ser_action.ForRead()) {
            init();
        }
        READWRITE(version);
        READWRITE(services);
        READWRITE(timestamp);
        READWRITE(addrMe);
        if (s.GetVersion() >= 106) {
            READWRITE(addrYou);
            READWRITE(nonce);
            READWRITE(user_agent);
            READWRITE(start_height);
        }
        if (s.GetVersion() >=70001) {
            READWRITE(relay);
        }
    }
    void init() {
        bzero(this, sizeof(*this));
    }
    uint32_t version;
    uint64_t services;
    int64_t timestamp;
    CAddress addrMe;
    // following requires version >= 106
    CAddress addrYou;
    uint64_t nonce;
    std::string user_agent;
    uint32_t start_height;
    // following requires version >= 70001
    bool relay;
};



#endif