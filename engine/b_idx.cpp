#include "b_idx.h"
#include "Tokenizer.h"
#include "Stemmer.h"
#include <algorithm>

void BooleanIndex::addDocument(const Document& doc) {
    docs_count_ = std::max(docs_count_, (size_t)(doc.id + 1));
    all_docs_.push_back(doc.id);

    std::vector<std::string> terms;
    terms.reserve(2048);

    auto tokens = Tokenizer::tokenize(doc.text);
    for (const auto& t : tokens) {
        std::string term = Stemmer::stem(t);
        if (term.size() < 2) continue;   
        terms.push_back(std::move(term));
    }

    std::sort(terms.begin(), terms.end());
    terms.erase(std::unique(terms.begin(), terms.end()), terms.end());

    for (const auto& term : terms) {
        table_.getOrInsert(term).push_back(doc.id);
    }
}

void BooleanIndex::finalize() {
    std::sort(all_docs_.begin(), all_docs_.end());
    all_docs_.erase(std::unique(all_docs_.begin(), all_docs_.end()), all_docs_.end());

    table_.forEach([&](const std::string&, std::vector<int>& lst) {
        std::sort(lst.begin(), lst.end());
        lst.erase(std::unique(lst.begin(), lst.end()), lst.end());
    });
}

const std::vector<int>& BooleanIndex::postings(const std::string& term) const {
    static const std::vector<int> empty;
    if (auto p = table_.find(term)) return *p;
    return empty;
}