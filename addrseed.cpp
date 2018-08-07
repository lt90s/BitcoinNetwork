#include "addrseed.h"
#include "reporter.h"

#include <bitcoin/protocol.h>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <arpa/inet.h>


extern ReporterInterface *gReporter;

CAddrSeed *CAddrSeed::mInstance = nullptr;

void CAddrSeed::addNewAddr(const CService &addr)
{
    std::lock_guard<std::mutex> lock(mSeedLock);
    auto pair = mSeenAddr.insert(addr);
    // duplicate address
    if (!pair.second) {
        return;
    }
    if (gReporter != nullptr) {
        gReporter->reportNewAddr(addr.ToStringIP(), addr.GetPort());
    }
    bool need_notify = false;
    if (mSeedAddr.empty()) {
        need_notify = true;
    }
    mSeedAddr.push_back(&(*pair.first));

    if (need_notify) {
        mCond.notify_one();
    }
}


bool CAddrSeed::getNewAddrs(std::vector<CService *> &addrs, size_t &size, bool wait) {
    std::unique_lock<std::mutex> lock(mSeedLock);


    if (size <= 0) {
        return false;
    }

    if (mSeedAddr.empty()) {
        size = 0;
        if (wait) {
            mCond.wait(lock, [this] { return !mSeedAddr.empty(); });
        } else {
            return false;
        }
    }

    size = std::min(size, mSeedAddr.size());
    for (auto i = 0; i < size; ++i) {
        addrs.push_back(const_cast<CService *>(mSeedAddr.front()));
        mSeedAddr.pop_front();
    }

    return true;
}

#if defined(MAIN_ADDRSEED)
int main(void)
{
    CAddrSeed &addrSeed = CAddrSeed::getInstance();
    struct in_addr inAddr;
    inet_pton(AF_INET, "192.168.0.1", &inAddr);
    auto addr = CService(inAddr, 8333);
    printf("addr: %s\n", addr.ToString().c_str());
    addrSeed.addNewAddr(addr);

    std::vector<CService *> vAddrs;
    size_t size = 2;
    addrSeed.getNewAddrs(vAddrs, size, false);
    for (auto paddr: vAddrs) {
        printf("addr: %s\n", addr.ToString().c_str());
    }
}
#endif