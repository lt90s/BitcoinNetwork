#ifndef __REPORTER_H__
#define __REPORTER_H__

#include <string>
#include <stdint.h>
#include <thread>

class ReporterInterface
{
public:
    virtual void reportNewAddr(const std::string &saddr, uint16_t port) = 0;
    // virtual void reportConnectedAddr(std::string saddr, uint16_t) = 0;
    virtual void reportVersionedAddr(const std::string saddr, uint16_t, const std::string agent, uint32_t version, uint64_t services) = 0;
    // void reportUnConnectedAddr(std::string saddr, uint16_t) = 0;
    virtual std::thread runThread() = 0;
    virtual ~ReporterInterface() = default;
};

#endif