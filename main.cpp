#include "init.h"
#include "network.h"
#include "http_reporter.h"

#include <signal.h>
#include <thread>
#include <iostream>

static const std::vector<std::string> seedNodes = {
    "seed.bitcoin.sipa.be",
    "dnsseed.bluematt.me",
    "dnsseed.bitcoin.dashjr.org",
    "seed.bitcoinstats.com",
    "seed.bitcoin.jonasschnelli.ch",
    "seed.btc.petertodd.org",
    "seed.bitcoin.sprovoost.nl"
};

static const char *reportAPI = "http://127.0.0.1:8000/bitcoin_network/report";

void ignore(int sig)
{
    printf("ignore signal %d\n", sig);
}

ReporterInterface *gReporter = nullptr;

int main(int argc, char *argv[])
{
    HttpReporter hp("http://127.0.0.1:8888/bitcoin_network/report");
    gReporter = &hp;
    std::thread t = gReporter->runThread();

    initDNSSeedAddr(seedNodes);
    NetworkEngine engine(70015);
    if (!engine.initEngine()) {
        printf("network engine init failed, exit!\n");
        return -1;
    }

    signal(SIGPIPE, ignore);
    engine.startEngine();
}