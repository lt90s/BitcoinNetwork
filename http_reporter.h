#ifndef __HTTP_REPORTER_H__
#define __HTTP_REPORTER_H__

#include "reporter.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <curl/curl.h>

class HttpReporter: public ReporterInterface
{
public:
    explicit HttpReporter(const std::string &apiUrlIn): ReporterInterface(), apiUrl(apiUrlIn), easy(nullptr) {}
    void reportNewAddr(const std::string &saddr, uint16_t port);
    void reportVersionedAddr(const std::string saddr, uint16_t, const std::string agent, uint32_t version, uint64_t services);
    std::thread runThread();
    ~HttpReporter() {
        if (easy != nullptr) {
            curl_easy_cleanup(easy);
        }
    }
    
private:
    struct VersionedAddr {
        std::string addr;
        uint16_t port;
        std::string agent;
        uint32_t version;
        uint64_t services;
    };
    void httpReporterThread();
    std::string apiUrl;
    std::vector<std::pair<std::string, uint16_t> > newAddrs;
    std::vector<VersionedAddr> vVersionAddrs;
    std::mutex newAddrsMutex;
    std::mutex vVersionAddrsMutex;
    CURL *easy;
};

void HttpReporter::reportNewAddr(const std::string &saddr, uint16_t port)
{
    std::lock_guard<std::mutex> lock(newAddrsMutex);
    newAddrs.push_back(std::make_pair(saddr, port));
}

void HttpReporter::reportVersionedAddr(const std::string saddr, uint16_t port, const std::string agent, uint32_t version, uint64_t services)
{
    VersionedAddr va = { saddr, port, agent, version, services };
    std::lock_guard<std::mutex> lock(vVersionAddrsMutex);
    vVersionAddrs.push_back(va);
}

std::thread HttpReporter::runThread()
{
    return std::thread(std::bind(&HttpReporter::httpReporterThread, this));
}

void HttpReporter::httpReporterThread()
{
    easy = curl_easy_init();
    while (true) {
        sleep(1);
        std::vector<std::pair<std::string, uint16_t> > newAddrsCopy;
        {
            std::lock_guard<std::mutex> lock(newAddrsMutex);
            newAddrsCopy.swap(newAddrs);
        }
        char port[6];
        for (auto addrPair: newAddrsCopy) {
            memset(port, 0, sizeof(addrPair.second));
            curl_mime *mime = curl_mime_init(easy);
            curl_mimepart *part = curl_mime_addpart(mime);
            curl_mime_data(part, addrPair.first.c_str(), CURL_ZERO_TERMINATED);
            curl_mime_name(part, "ip");
            
            part = curl_mime_addpart(mime);
            sprintf(port, "%d", addrPair.second);
            curl_mime_data(part, port, CURL_ZERO_TERMINATED);
            curl_mime_name(part, "port");

            part = curl_mime_addpart(mime);
            curl_mime_data(part, "new", CURL_ZERO_TERMINATED);
            curl_mime_name(part, "type");
    
            /* Post and send it. */
            curl_easy_setopt(easy, CURLOPT_MIMEPOST, mime);
            curl_easy_setopt(easy, CURLOPT_URL, apiUrl.c_str());
            curl_easy_perform(easy);
            curl_mime_free(mime);
        }

        std::vector<VersionedAddr> vVersionAddrsCopy;
        {
            std::lock_guard<std::mutex> lock(vVersionAddrsMutex);
            vVersionAddrsCopy.swap(vVersionAddrs);
        }
        char buffer[16];
        for (auto va: vVersionAddrsCopy) {
            memset(buffer, 0, sizeof(buffer));
            curl_mime *mime = curl_mime_init(easy);
            curl_mimepart *part = curl_mime_addpart(mime);
            curl_mime_data(part, va.addr.c_str(), CURL_ZERO_TERMINATED);
            curl_mime_name(part, "ip");
            
            part = curl_mime_addpart(mime);
            sprintf(buffer, "%d", va.port);
            curl_mime_data(part, buffer, CURL_ZERO_TERMINATED);
            curl_mime_name(part, "port");

            part = curl_mime_addpart(mime);
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "%u", va.version);
            curl_mime_data(part, buffer, CURL_ZERO_TERMINATED);
            curl_mime_name(part, "version");

            part = curl_mime_addpart(mime);
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "%lu", va.services);
            curl_mime_data(part, buffer, CURL_ZERO_TERMINATED);
            curl_mime_name(part, "services");

            part = curl_mime_addpart(mime);
            curl_mime_data(part, va.agent.c_str(), CURL_ZERO_TERMINATED);
            curl_mime_name(part, "agent");

            part = curl_mime_addpart(mime);
            curl_mime_data(part, "version", CURL_ZERO_TERMINATED);
            curl_mime_name(part, "type");

            /* Post and send it. */
            curl_easy_setopt(easy, CURLOPT_MIMEPOST, mime);
            curl_easy_setopt(easy, CURLOPT_URL, apiUrl.c_str());
            curl_easy_perform(easy);
            curl_mime_free(mime);
        }
    }
}

#endif