#include "storage/pager.h"
#include <cstring>
#include <stdexcept>

namespace minidb {

Pager::Pager(const std::string& filename) {
    // Try to open existing file for read+write
    file_ = fopen(filename.c_str(), "rb+");
    if (!file_) {
        // File doesn't exist — create it
        file_ = fopen(filename.c_str(), "wb+");
    }
    if (!file_) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    // Figure out how many pages already exist
    fseek(file_, 0, SEEK_END);
    long file_size = ftell(file_);
    num_pages_ = file_size / PAGE_SIZE;

    // All cache slots start empty
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        pages_[i] = nullptr;
    }
}

Pager::~Pager() {
    flushAll();
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        delete[] pages_[i];
    }
    if (file_) fclose(file_);
}

uint8_t* Pager::getPage(uint32_t page_num) {
    if (page_num >= MAX_PAGES) {
        throw std::runtime_error("Page number out of bounds: " + std::to_string(page_num));
    }

    // Cache hit — already in memory
    if (pages_[page_num] != nullptr) {
        return pages_[page_num];
    }

    // Cache miss — allocate memory and load from disk (or create blank)
    uint8_t* page = new uint8_t[PAGE_SIZE];
    memset(page, 0, PAGE_SIZE);

    if (page_num < num_pages_) {
        // Page exists on disk — read it
        fseek(file_, (long)page_num * PAGE_SIZE, SEEK_SET);
        fread(page, 1, PAGE_SIZE, file_);
    }

    pages_[page_num] = page;
    if (page_num >= num_pages_) {
        num_pages_ = page_num + 1;
    }

    return page;
}

void Pager::flushPage(uint32_t page_num) {
    if (page_num >= MAX_PAGES || pages_[page_num] == nullptr) return;

    fseek(file_, (long)page_num * PAGE_SIZE, SEEK_SET);
    fwrite(pages_[page_num], 1, PAGE_SIZE, file_);
    fflush(file_);
}

uint32_t Pager::allocatePage() {
    uint32_t page_num = num_pages_;
    getPage(page_num);  // this creates a blank page and increments num_pages_
    return page_num;
}

void Pager::flushAll() {
    for (uint32_t i = 0; i < num_pages_; i++) {
        flushPage(i);
    }
}

} // namespace minidb
