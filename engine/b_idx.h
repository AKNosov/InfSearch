#pragma once
#include <string>
#include <vector>
#include "HashTable.h"

struct Document {
    int id;
    std::string key;   
    std::string text;  
};

class BooleanIndex {
public:
    BooleanIndex() = default;

    void addDocument(const Document& doc);
    void finalize();

    const std::vector<int>& postings(const std::string& term) const;
    const std::vector<int>& allDocs() const { return all_docs_; }

    size_t docsCount() const { return docs_count_; }
    size_t termsCount() const { return table_.size(); }

private:
    size_t docs_count_ = 0;
    std::vector<int> all_docs_;
    HashTable table_;
};