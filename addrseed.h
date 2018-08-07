#ifndef __ADDRSET_H__
#define __ADDRSET_H__

#include <bitcoin/protocol.h>

#include <set>
#include <deque>
#include <mutex>
#include <condition_variable>

static std::mutex instanceLock;
class CAddrSeed
{
public:
    CAddrSeed(const CAddrSeed &) = delete;
    CAddrSeed& operator=(const CAddrSeed &) = delete;
    static CAddrSeed &getInstance() {
        if (mInstance == nullptr) {
            std::lock_guard<std::mutex> lock(instanceLock);
            mInstance = new CAddrSeed;
        }
        return *mInstance;
    }
    void addNewAddr(const CService &addr);
    bool getNewAddrs(std::vector<CService *> &addrs, size_t &size, bool wait=false);

private:
    CAddrSeed() = default;    
    static CAddrSeed *mInstance;

    std::condition_variable mCond;
    std::mutex mSeedLock;
    std::deque<const CService *> mSeedAddr;
    std::set<CService> mSeenAddr;
    std::vector<const CService *> mTimeoutAddr;
    std::vector<const CService *> mRefusedAddr;
};

#endif
