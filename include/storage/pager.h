#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace minidb {

// Page size = 4096 bytes (same as OS disk block size for efficient I/O)
static constexpr uint32_t PAGE_SIZE = 4096;
static constexpr uint32_t MAX_PAGES = 1000;

// Pager: manages reading/writing a file in fixed-size 4096-byte chunks (pages).
//
// Think of it like a notebook:
//   Page 0 = first 4096 bytes of the file
//   Page 1 = next 4096 bytes
//   Page 2 = next 4096 bytes
//   ...
//
// Instead of doing raw fseek/fread everywhere, you just say:
//   uint8_t* data = pager.getPage(2);   // gives you page 2
//   pager.flushPage(2);                 // saves page 2 to disk
//
// It also caches pages in memory so repeated reads are fast.

class Pager {
public:
    explicit Pager(const std::string& filename);
    ~Pager();

    // Get a page (loads from disk if not cached, creates blank if new)
    uint8_t* getPage(uint32_t page_num);

    // Write a cached page back to the disk file
    void flushPage(uint32_t page_num);

    // Allocate a new page (returns its page number)
    uint32_t allocatePage();

    // Write all cached pages to disk
    void flushAll();

    uint32_t getNumPages() const { return num_pages_; }

private:
    FILE* file_;
    uint8_t* pages_[MAX_PAGES];   // Cache: pages_[i] = 4096 bytes or nullptr
    uint32_t num_pages_;           // How many pages the file has
};

// Helper functions to read/write integers from a page at a given byte offset.
// We use these everywhere to read structured data from pages.
//
// Example: writeU32(page, 4, 42) writes the number 42 at bytes 4-7 of the page
//          readU32(page, 4) reads it back → 42

inline uint32_t readU32(const uint8_t* page, uint32_t offset) {
    uint32_t val;
    __builtin_memcpy(&val, page + offset, sizeof(uint32_t));
    return val;
}

inline void writeU32(uint8_t* page, uint32_t offset, uint32_t val) {
    __builtin_memcpy(page + offset, &val, sizeof(uint32_t));
}

inline uint16_t readU16(const uint8_t* page, uint32_t offset) {
    uint16_t val;
    __builtin_memcpy(&val, page + offset, sizeof(uint16_t));
    return val;
}

inline void writeU16(uint8_t* page, uint32_t offset, uint16_t val) {
    __builtin_memcpy(page + offset, &val, sizeof(uint16_t));
}

inline int32_t readI32(const uint8_t* page, uint32_t offset) {
    int32_t val;
    __builtin_memcpy(&val, page + offset, sizeof(int32_t));
    return val;
}

inline void writeI32(uint8_t* page, uint32_t offset, int32_t val) {
    __builtin_memcpy(page + offset, &val, sizeof(int32_t));
}

} // namespace minidb
