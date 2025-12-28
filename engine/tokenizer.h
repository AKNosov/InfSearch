#pragma once
#include <string>
#include <vector>

class Tokenizer {
public:
    static std::vector<std::string> tokenize(const std::string& utf8);

private:
    static bool startsWith(const std::string& s, size_t i, const char* lit);
    static bool isUrlStart(const std::string& s, size_t i);
    static bool isEmailStartOrInside(const std::string& s, size_t i); // '@'
    static size_t skipUntilWhitespace(const std::string& s, size_t i);
    enum class CpType { Word, Joiner, Other };

    struct Cp {
        CpType type;
        char b1 = 0;
        char b2 = 0;
        int bytes = 0;     
        int chars = 0;     
        bool isDigit = false;
        bool isLetter = false;
        bool isJoinerHyphen = false;
        bool isJoinerApos = false;
    };

    static Cp readCp(const std::string& s, size_t i);

    static unsigned char asciiLower(unsigned char c);

    static bool normalizeCyr2(unsigned char lead, unsigned char trail,
                              unsigned char& nLead, unsigned char& nTrail);

    static bool isAsciiWord(unsigned char c); 
};