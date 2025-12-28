#include "HashTable.h"
#include <algorithm>

static size_t nextPow2(size_t x) { size_t p=1; while (p<x) p<<=1; return p; }

uint64_t HashTable::hash64(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) { h ^= (uint64_t)c; h *= 1099511628211ull; }
    return h;
}

HashTable::HashTable(size_t initialCapPow2) {
    size_t cap = nextPow2(std::max<size_t>(8, initialCapPow2));
    entries_.resize(cap);
    mask_ = cap - 1;
}

size_t HashTable::probeIndex(const std::string& key) const {
    size_t idx = (size_t)hash64(key) & mask_;
    while (true) {
        const auto& e = entries_[idx];
        if (e.state == State::EMPTY) return idx;
        if (e.key == key) return idx;
        idx = (idx + 1) & mask_;
    }
}

void HashTable::rehash(size_t newCapPow2) {
    std::vector<Entry> old = std::move(entries_);
    entries_.assign(newCapPow2, Entry{});
    mask_ = newCapPow2 - 1;
    size_ = 0;

    for (auto& e : old) {
        if (e.state != State::FILLED) continue;
        size_t idx = probeIndex(e.key);
        entries_[idx].state = State::FILLED;
        entries_[idx].key = std::move(e.key);
        entries_[idx].value = std::move(e.value);
        size_++;
    }
}

std::vector<int>& HashTable::getOrInsert(const std::string& key) {
    if ((double)(size_ + 1) / (double)entries_.size() > maxLoad_) {
        rehash(entries_.size() * 2);
    }
    size_t idx = probeIndex(key);
    auto& e = entries_[idx];
    if (e.state == State::EMPTY) {
        e.state = State::FILLED;
        e.key = key;
        e.value.clear();
        size_++;
    }
    return e.value;
}

const std::vector<int>* HashTable::find(const std::string& key) const {
    size_t idx = (size_t)hash64(key) & mask_;
    while (true) {
        const auto& e = entries_[idx];
        if (e.state == State::EMPTY) return nullptr;
        if (e.key == key) return &e.value;
        idx = (idx + 1) & mask_;
    }
}