#pragma once

#include "Plugin.h"
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <atomic>
#include <thread>

// page alignment helpers
#define PAGE_SIZE  0x1000
#define PAGE_MASK  0xFFF
#define PAGE_SHIFT 0xC

#define BYTE_OFFSET(Address)  ((size_t)((uintptr_t)(Address) & PAGE_MASK))
#define PAGE_ALIGN(Address)   ((uintptr_t)((uintptr_t)(Address) & ~PAGE_MASK))
#define PAGE_BASE(Address)    (uintptr_t)((uintptr_t)(Address) & ~(uintptr_t)(PAGE_SIZE - 1))
#define PAGE_OFFSET(p)        ((PAGE_MASK) & (uintptr_t)(p))

struct ReadKey {
    uintptr_t page_base; // page-aligned base
    bool operator<(const ReadKey& other) const { return page_base < other.page_base; }

    ReadKey(uintptr_t address) {
        page_base = PAGE_ALIGN(address);
    }
};

// timestamp type for cache expiry
using cache_timestamp = std::chrono::steady_clock::time_point;

// cache expiring configuration. Tweak as needed for performance vs freshness
constexpr std::chrono::milliseconds CACHE_EXPIRY_DURATION(16); // ~60fps freshness

// returns true if entry is expired
using cache_expiry_handler = bool (*)(const cache_timestamp& timestamp);

constexpr cache_expiry_handler DEFAULT_CACHE_EXPIRY_HANDLER =
[](const cache_timestamp& timestamp) {
    return (std::chrono::steady_clock::now() - timestamp) > CACHE_EXPIRY_DURATION;
    };

struct CachedBlock {
    // page aligned data buffer. WARNING: CAN BE PARTIALLY FILLED.
    // USE valid_start and valid_length TO KNOW WHICH PART IS VALID
    uint8_t data[PAGE_SIZE];

    // offset in page where data is valid
    size_t valid_start = 0;

    // length of valid region from valid_start
    size_t valid_length = 0;

    cache_timestamp timestamp{};
};

// hope that the data is stored contiguously; can be swapped later if needed
extern std::map<ReadKey, CachedBlock> g_cache;

// mutex protecting the cache. Shared mutex to allow multiple concurrent readers
extern std::shared_mutex g_cache_mutex;

// Job queue types for the cached reader worker thread
enum class JobType {
    Read,
    Write,
    OpenProcess,
    CloseProcess
};

struct Job {
    JobType   type;
    uint32_t  pid;

    union {
        struct {
            uintptr_t address;
            size_t    size;
        } read;
        struct {
            uintptr_t address;
        } write;
        // Open/Close process need only pid for now
    };

    // payload (for write etc.)
    std::vector<uint8_t> data;
};

extern std::queue<Job> g_jobs;
extern std::mutex      g_jobs_mutex;

extern std::atomic<bool> g_workerRunning;
extern std::thread* g_workerThread;

bool StartWorker();
void StopWorker();
