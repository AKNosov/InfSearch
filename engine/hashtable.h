#pragma once
#include <string>
#include <vector>
#include <cstdint>

class HashTable {
public:
    HashTable(size_t initialCapPow2 = 1 << 20);
    std::vector<int>& getOrInsert(const std::string& key);

    const std::vector<int>* find(const std::string& key) const;

    size_t size() const { return size_; }

    template <class F>
    void forEach(F&& f) {
        for (auto& e : entries_) if (e.state == State::FILLED) f(e.key, e.value);
    }

private:
    enum class State : uint8_t { EMPTY, FILLED };

    struct Entry {
        State state = State::EMPTY;
        std::string key;
        std::vector<int> value;
    };

    std::vector<Entry> entries_;
    size_t size_ = 0;
    size_t mask_ = 0;
    double maxLoad_ = 0.70;

    static uint64_t hash64(const std::string& s); 
    size_t probeIndex(const std::string& key) const;
    void rehash(size_t newCapPow2);
};