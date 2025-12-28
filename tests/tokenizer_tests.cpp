#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "../engine/tokenizer.h"

static int g_failed = 0;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " ASSERT_TRUE(" #cond ")\n"; \
        g_failed++; return; \
    } \
} while(0)

static bool contains(const std::vector<std::string>& v, const std::string& x) {
    for (const auto& s : v) if (s == x) return true;
    return false;
}

static void assertContainsAll(const std::vector<std::string>& got, const std::vector<std::string>& must) {
    for (const auto& m : must) {
        if (!contains(got, m)) {
            std::cerr << "[FAIL] missing token: " << m << "\n";
            std::cerr << "  got:";
            for (auto& t : got) std::cerr << " {" << t << "}";
            std::cerr << "\n";
            g_failed++;
            return;
        }
    }
}

static void assertNotContainsAny(const std::vector<std::string>& got, const std::vector<std::string>& bad) {
    for (const auto& b : bad) {
        if (contains(got, b)) {
            std::cerr << "[FAIL] should NOT contain token: " << b << "\n";
            std::cerr << "  got:";
            for (auto& t : got) std::cerr << " {" << t << "}";
            std::cerr << "\n";
            g_failed++;
            return;
        }
    }
}

static void run(const char* name, void(*fn)()) {
    int before = g_failed;
    fn();
    if (g_failed == before) std::cerr << "[OK]   " << name << "\n";
}

static void test_basic_separators_and_lower() {
    auto t = Tokenizer::tokenize("Привет, Мир! ABC DEF.");
    assertContainsAll(t, {"привет", "мир", "abc", "def"});
}

static void test_numbers_preserved() {
    auto t = Tokenizer::tokenize("В 2025 году было 12 событий, 3.14 не токен.");

    assertContainsAll(t, {"2025", "12"});
}

static void test_min_max_len() {
    std::string longword(60, 'a');
    auto t = Tokenizer::tokenize("a аб " + longword + " ok");
    ASSERT_TRUE(!contains(t, "a"));
    ASSERT_TRUE(contains(t, "аб"));
    ASSERT_TRUE(contains(t, "ok"));
    ASSERT_TRUE(!contains(t, longword));
}

static void test_skip_url_and_email() {
    auto t = Tokenizer::tokenize("см https://example.com/x?a=1 и test@mail.com и www.site.ru ok");
    assertNotContainsAny(t, {
        "https", "example", "com", "test", "mail", "www", "site", "ru", "x", "a"
    });
    ASSERT_TRUE(contains(t, "см") || contains(t, "ok"));
}

static void test_hyphen_kept_inside_word_and_parts_present() {
    auto t = Tokenizer::tokenize("Санкт-Петербург — красивый город.");
    assertContainsAll(t, {"санкт-петербург", "санктпетербург", "санкт", "петербург", "красивый", "город"});
}

static void test_unicode_dash_is_hyphen() {
    auto t = Tokenizer::tokenize("научно—практический научно–практический");
    assertContainsAll(t, {"научно-практический", "научнопрактический", "научно", "практический"});
}

static void test_apostrophe_handling_ascii_and_unicode() {
    auto t = Tokenizer::tokenize("don't rock’n’roll");
    bool okDont = contains(t, "don't") || contains(t, "dont");
    ASSERT_TRUE(okDont);

    ASSERT_TRUE(contains(t, "rock'n'roll"));
    ASSERT_TRUE(contains(t, "rocknroll"));
    ASSERT_TRUE(contains(t, "rock"));
    ASSERT_TRUE(contains(t, "roll"));
    ASSERT_TRUE(!contains(t, "n"));
}

static void test_joiners_at_edges_are_delimiters() {
    auto t = Tokenizer::tokenize("-слово слово- 'test test'");
    assertContainsAll(t, {"слово", "test"});
}

static void test_yo_to_e_and_cyrillic_upper_to_lower() {
    auto t = Tokenizer::tokenize("ЁЛКА ёлка ЕЛКА");
    ASSERT_TRUE(contains(t, "елка"));
    ASSERT_TRUE(!contains(t, "ёлка"));
}

int main() {
    run("basic_separators_and_lower", test_basic_separators_and_lower);
    run("numbers_preserved", test_numbers_preserved);
    run("min_max_len", test_min_max_len);
    run("skip_url_and_email", test_skip_url_and_email);
    run("hyphen_kept_inside_word_and_parts_present", test_hyphen_kept_inside_word_and_parts_present);
    run("unicode_dash_is_hyphen", test_unicode_dash_is_hyphen);
    run("apostrophe_handling_ascii_and_unicode", test_apostrophe_handling_ascii_and_unicode);
    run("joiners_at_edges_are_delimiters", test_joiners_at_edges_are_delimiters);
    run("yo_to_e_and_cyrillic_upper_to_lower", test_yo_to_e_and_cyrillic_upper_to_lower);

    if (g_failed) {
        std::cerr << "\nFAILED: " << g_failed << "\n";
        return 1;
    }
    std::cerr << "\nALL TOKENIZER TESTS PASSED\n";
    return 0;
}