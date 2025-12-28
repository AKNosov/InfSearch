#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "../engine/stemmer.h"

static int g_failed = 0;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " ASSERT_TRUE(" #cond ")\n"; \
        g_failed++; return; \
    } \
} while(0)

#define ASSERT_EQ(a,b) do { \
    auto _a = (a); auto _b = (b); \
    if (!(_a == _b)) { \
        std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " ASSERT_EQ\n"; \
        std::cerr << "  left:  " << _a << "\n"; \
        std::cerr << "  right: " << _b << "\n"; \
        g_failed++; return; \
    } \
} while(0)

static void run(const char* name, void(*fn)()) {
    int before = g_failed;
    fn();
    if (g_failed == before) std::cerr << "[OK]   " << name << "\n";
}


static void test_english_porter_classic_set() {
    ASSERT_EQ(Stemmer::stem("caresses"), "caress");
    ASSERT_EQ(Stemmer::stem("ponies"),   "poni");
    ASSERT_EQ(Stemmer::stem("ties"),     "ti");
    ASSERT_EQ(Stemmer::stem("caress"),   "caress");
    ASSERT_EQ(Stemmer::stem("cats"),     "cat");

    ASSERT_EQ(Stemmer::stem("feed"),     "feed");
    ASSERT_EQ(Stemmer::stem("agreed"),   "agre");
    ASSERT_EQ(Stemmer::stem("disabled"), "disabl");

    ASSERT_EQ(Stemmer::stem("matting"),  "mat");
    ASSERT_EQ(Stemmer::stem("mating"),   "mate");
    ASSERT_EQ(Stemmer::stem("meeting"),  "meet");
    ASSERT_EQ(Stemmer::stem("milling"),  "mill");
    ASSERT_EQ(Stemmer::stem("messing"),  "mess");
    ASSERT_EQ(Stemmer::stem("meetings"), "meet");
}


static void assertSameStem(const std::vector<std::string>& forms) {
    ASSERT_TRUE(!forms.empty());
    std::string base = Stemmer::stem(forms[0]);
    ASSERT_TRUE(!base.empty());
    for (size_t i = 1; i < forms.size(); i++) {
        std::string s = Stemmer::stem(forms[i]);
        if (s != base) {
            std::cerr << "[FAIL] stems differ\n";
            std::cerr << "  base(" << forms[0] << ")=" << base << "\n";
            std::cerr << "  got (" << forms[i] << ")=" << s << "\n";
            g_failed++;
            return;
        }
    }
}

static void test_russian_same_stem_groups() {
    assertSameStem({"машина", "машины", "машиной", "машину", "машине"});
    assertSameStem({"возможность", "возможности", "возможностью"});
    assertSameStem({"реализация", "реализации", "реализацией"});
    assertSameStem({"документ", "документы", "документа", "документом"});
    assertSameStem({"поиск", "поиска", "поиском", "поиске"});
    assertSameStem({"индексация", "индексации", "индексацией"});
}

static void test_russian_yo_normalization_effect() {
    std::string a = Stemmer::stem("елка");
    std::string b = Stemmer::stem("ёлка"); 
    ASSERT_TRUE(!a.empty());
    ASSERT_TRUE(!b.empty());
}

static void test_hyphen_apostrophe_parts_are_stemmed() {
    std::string s1 = Stemmer::stem("санкт-петербург");
    ASSERT_TRUE(!s1.empty());
    ASSERT_TRUE(s1.find('-') != std::string::npos);

    std::string s2 = Stemmer::stem("rock'n'roll");
    ASSERT_TRUE(!s2.empty());
    ASSERT_TRUE(s2.find('\'') != std::string::npos);
}

static void test_numbers_and_mixed_tokens_unchanged_or_safe() {
    ASSERT_EQ(Stemmer::stem("2025"), "2025");
    ASSERT_EQ(Stemmer::stem("rbc.ru"), "rbc.ru"); 
    ASSERT_EQ(Stemmer::stem("covid19"), "covid19"); 
}

int main() {
    run("english_porter_classic_set", test_english_porter_classic_set);
    run("russian_same_stem_groups", test_russian_same_stem_groups);
    run("russian_yo_normalization_effect", test_russian_yo_normalization_effect);
    run("hyphen_apostrophe_parts_are_stemmed", test_hyphen_apostrophe_parts_are_stemmed);
    run("numbers_and_mixed_tokens_unchanged_or_safe", test_numbers_and_mixed_tokens_unchanged_or_safe);

    if (g_failed) {
        std::cerr << "\nFAILED: " << g_failed << "\n";
        return 1;
    }
    std::cerr << "\nALL STEMMER TESTS PASSED\n";
    return 0;
}