#include <iostream>
#include <string>
#include <vector>

#include "../engine/tokenizer.h"
#include "../engine/stemmer.h"
#include "../engine/hashTable.h"
#include "../engine/b_idx.h"
#include "../engine/b_srch.h"

static int g_failed = 0;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " ASSERT_TRUE(" #cond ")\n"; \
        g_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a,b) do { \
    auto _a = (a); auto _b = (b); \
    if (!(_a == _b)) { \
        std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " ASSERT_EQ\n"; \
        std::cerr << "  left:  " << _a << "\n"; \
        std::cerr << "  right: " << _b << "\n"; \
        g_failed++; \
        return; \
    } \
} while(0)

static bool vecEq(const std::vector<int>& a, const std::vector<int>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i=0;i<a.size();i++) if (a[i] != b[i]) return false;
    return true;
}

static bool contains(const std::vector<std::string>& v, const std::string& x) {
    for (auto& s : v) if (s == x) return true;
    return false;
}

static void printVec(const std::vector<int>& v) {
    std::cerr << "[";
    for (size_t i=0;i<v.size();i++) {
        std::cerr << v[i];
        if (i+1<v.size()) std::cerr << ",";
    }
    std::cerr << "]";
}


static void test_tokenizer_basic() {
    auto t = Tokenizer::tokenize("Привет, Мир! ABC 123.");
    ASSERT_TRUE(contains(t, "привет"));
    ASSERT_TRUE(contains(t, "мир"));
    ASSERT_TRUE(contains(t, "abc"));
    ASSERT_TRUE(contains(t, "123")); 
}

static void test_tokenizer_min_max_len() {
    auto t = Tokenizer::tokenize("a аб " + std::string(60, 'b'));
    ASSERT_TRUE(!contains(t, "a"));
    ASSERT_TRUE(contains(t, "аб"));
    ASSERT_TRUE(!contains(t, std::string(60, 'b')));
}

static void test_tokenizer_skip_url_email() {
    auto t = Tokenizer::tokenize("см. https://example.com/path?q=1 и test@mail.com а также www.site.ru");
    ASSERT_TRUE(!contains(t, "https"));
    ASSERT_TRUE(!contains(t, "example"));
    ASSERT_TRUE(!contains(t, "com"));
    ASSERT_TRUE(!contains(t, "test"));
    ASSERT_TRUE(!contains(t, "mail"));
    ASSERT_TRUE(!contains(t, "www"));
    ASSERT_TRUE(!contains(t, "site"));
}

static void test_tokenizer_hyphen_apostrophe() {
    auto t = Tokenizer::tokenize("Санкт-Петербург don't rock'n'roll");
    ASSERT_TRUE(contains(t, "санкт-петербург"));
    bool ok = contains(t, "don't") || contains(t, "dont");
    ASSERT_TRUE(ok);
}

static void test_stemmer_english_porter() {
    std::string a = Stemmer::stem("running");
    std::string b = Stemmer::stem("studies");
    ASSERT_TRUE(a == "run" || a == "runn"); 
    ASSERT_TRUE(b == "studi" || b == "study");
}

static void test_stemmer_russian_porter() {
    std::string s1 = Stemmer::stem("машины");
    std::string s2 = Stemmer::stem("машина");
    ASSERT_TRUE(!s1.empty());
    ASSERT_TRUE(!s2.empty());
    ASSERT_TRUE(s1 == s2);
}


static void test_hashtable_insert_find() {
    HashTable ht(8);
    ht.getOrInsert("a").push_back(1);
    ht.getOrInsert("a").push_back(2);
    ht.getOrInsert("b").push_back(7);

    auto pa = ht.find("a");
    auto pb = ht.find("b");
    auto pc = ht.find("c");

    ASSERT_TRUE(pa != nullptr);
    ASSERT_TRUE(pb != nullptr);
    ASSERT_TRUE(pc == nullptr);
    ASSERT_TRUE(pa->size() == 2);
    ASSERT_TRUE((*pb)[0] == 7);
}

static void test_hashtable_rehash() {
    HashTable ht(8);
    for (int i=0;i<200;i++) {
        ht.getOrInsert("k" + std::to_string(i)).push_back(i);
    }
    for (int i=0;i<200;i++) {
        auto p = ht.find("k" + std::to_string(i));
        ASSERT_TRUE(p != nullptr);
        ASSERT_TRUE(!p->empty());
        ASSERT_TRUE((*p)[0] == i);
    }
}

static BooleanIndex buildSmallIndex(std::vector<std::string>& urls) {
    std::vector<Document> docs;
    docs.push_back({0, "u0", "нефть и газ европа"});
    docs.push_back({1, "u1", "газ россия"});
    docs.push_back({2, "u2", "нефть санкции европа"});
    docs.push_back({3, "u3", "машины машина мотор"});
    urls = {"u0","u1","u2","u3"};

    BooleanIndex idx;
    for (auto& d : docs) idx.addDocument(d);
    idx.finalize();
    return idx;
}

static void test_boolean_index_postings() {
    std::vector<std::string> urls;
    auto idx = buildSmallIndex(urls);

    std::string t = Stemmer::stem("нефть");
    auto p = idx.postings(t);
    ASSERT_TRUE(p.size() >= 2); 
}

static void test_boolean_search_and_or_not_parentheses() {
    std::vector<std::string> urls;
    auto idx = buildSmallIndex(urls);
    BooleanSearch bs(idx);

    auto hits = bs.search("(нефть OR газ) AND NOT европа");

    ASSERT_TRUE(hits.size() == 1);
    ASSERT_TRUE(hits[0] == 1);
}

static void test_boolean_search_implicit_and() {
    std::vector<std::string> urls;
    auto idx = buildSmallIndex(urls);
    BooleanSearch bs(idx);

    auto hits = bs.search("нефть европа");
    ASSERT_TRUE(hits.size() == 2);
    ASSERT_TRUE(hits[0] == 0);
    ASSERT_TRUE(hits[1] == 2);
}

static void run(const char* name, void(*fn)()) {
    int before = g_failed;
    fn();
    if (g_failed == before) std::cerr << "[OK]   " << name << "\n";
}

int main() {
    run("tokenizer_basic", test_tokenizer_basic);
    run("tokenizer_min_max_len", test_tokenizer_min_max_len);
    run("tokenizer_skip_url_email", test_tokenizer_skip_url_email);
    run("tokenizer_hyphen_apostrophe", test_tokenizer_hyphen_apostrophe);

    run("stemmer_english_porter", test_stemmer_english_porter);
    run("stemmer_russian_porter", test_stemmer_russian_porter);

    run("hashtable_insert_find", test_hashtable_insert_find);
    run("hashtable_rehash", test_hashtable_rehash);

    run("boolean_index_postings", test_boolean_index_postings);
    run("boolean_search_and_or_not_parentheses", test_boolean_search_and_or_not_parentheses);
    run("boolean_search_implicit_and", test_boolean_search_implicit_and);

    if (g_failed) {
        std::cerr << "\nFAILED: " << g_failed << "\n";
        return 1;
    }
    std::cerr << "\nALL TESTS PASSED\n";
    return 0;
}




