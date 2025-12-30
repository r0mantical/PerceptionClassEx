#pragma once

#include "Plugin.h"
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <shared_mutex> 

struct ReadKey {
    uintptr_t address;
    size_t size;

    bool operator<(const ReadKey& other) const {
        return std::tie(address, size) < std::tie(other.address, other.size);
    }
};

// timestamp type for cache expiry
typedef std::chrono::steady_clock::time_point cache_timestamp;

// cache expiring configuration. Tweak as needed for performance vs freshness
constexpr std::chrono::milliseconds CACHE_EXPIRY_DURATION(10); // 10ms expiry for now

// the function which determines if a cache entry is expired
typedef bool (*cache_expiry_handler)(const cache_timestamp& timestamp);
constexpr cache_expiry_handler DEFAULT_CACHE_EXPIRY_HANDLER =
    [](const cache_timestamp& timestamp) {
        return (std::chrono::steady_clock::now() - timestamp) > CACHE_EXPIRY_DURATION;
    };

struct CachedBlock {
    std::vector<uint8_t> data;
    // optionally timestamp, validity flags, etc.
    //uint64_t protection_flags; // todo
    cache_timestamp timestamp;
};

extern std::map<ReadKey, CachedBlock> g_cache;
extern std::shared_mutex g_cache_mutex;

enum class JobType {
    Read,
    Write,
    OpenProcess,
    CloseProcess
};

struct Job {
    JobType type;
    uintptr_t address;
    std::vector<uint8_t> data; // for write
    //std::string processName;   // for open_process
	uint32_t pid;              // for open_process
};

extern std::queue<Job> g_jobs;
extern std::mutex g_jobs_mutex;

extern std::atomic<bool> g_workerRunning;
extern std::thread* g_workerThread;

//void worker_thread();
bool StartWorker();
void StopWorker();