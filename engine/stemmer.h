#pragma once
#include <string>

class Stemmer {
public:
    static std::string stem(const std::string& token);

private:
    static std::string stemRu(const std::string& token);
    static bool hasCyrillic(const std::string& s);
    static std::u16string toU16(const std::string& s);
    static std::string fromU16(const std::u16string& s);
    static bool isVowel(char16_t ch);
    static size_t findRV(const std::u16string& w);
    static size_t findR1(const std::u16string& w, size_t start);
    static size_t findR2(const std::u16string& w);

    static bool endsWith(const std::u16string& w, const std::u16string& suf);
    static bool removeSuffixInRegion(std::u16string& w, size_t region, const std::u16string& suf);
    static bool removeAnySuffixInRegion(std::u16string& w, size_t region, const std::u16string* sufs, size_t n);

    static bool removeIfPrecededByAY(std::u16string& w, size_t region, const std::u16string& suf);

    static std::string stemHyphenAposAware(const std::string& token);
};