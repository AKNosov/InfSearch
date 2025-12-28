#include "Tokenizer.h"
#include <cctype>
#include <unordered_set>

unsigned char Tokenizer::asciiLower(unsigned char c) {
    return (unsigned char)std::tolower(c);
}

bool Tokenizer::isAsciiWord(unsigned char c) {
    return std::isalnum(c) != 0;
}

bool Tokenizer::startsWith(const std::string& s, size_t i, const char* lit) {
    for (size_t k = 0; lit[k]; k++) {
        if (i + k >= s.size()) return false;
        if (s[i + k] != lit[k]) return false;
    }
    return true;
}

bool Tokenizer::isUrlStart(const std::string& s, size_t i) {
    return startsWith(s, i, "http://") || startsWith(s, i, "https://") || startsWith(s, i, "www.");
}

bool Tokenizer::isEmailStartOrInside(const std::string& s, size_t i) {
    return s[i] == '@';
}

size_t Tokenizer::skipUntilWhitespace(const std::string& s, size_t i) {
    while (i < s.size() && !std::isspace((unsigned char)s[i])) i++;
    return i;
}

bool Tokenizer::normalizeCyr2(unsigned char lead, unsigned char trail,
                              unsigned char& nLead, unsigned char& nTrail) {
    if (lead == 0xD0 && trail == 0x81) { nLead = 0xD0; nTrail = 0xB5; return true; }
    if (lead == 0xD1 && trail == 0x91) { nLead = 0xD0; nTrail = 0xB5; return true; }

    if (lead == 0xD0 && trail >= 0x90 && trail <= 0xAF) {
        nLead = 0xD0;
        nTrail = (unsigned char)(trail + 0x20);
        return true;
    }

    if (lead == 0xD0 && trail >= 0xB0 && trail <= 0xBF) {
        nLead = lead; nTrail = trail; return true;
    }

    if (lead == 0xD1 && trail >= 0x80 && trail <= 0x8F) {
        nLead = lead; nTrail = trail; return true;
    }

    return false;
}

Tokenizer::Cp Tokenizer::readCp(const std::string& s, size_t i) {
    Cp cp;

    unsigned char c = (unsigned char)s[i];

    if (c < 128) {
        if (c == '-') {
            cp.type = CpType::Joiner;
            cp.b1 = '-'; cp.bytes = 1; cp.chars = 1;
            cp.isJoinerHyphen = true;
            return cp;
        }
        if (c == '\'') {
            cp.type = CpType::Joiner;
            cp.b1 = '\''; cp.bytes = 1; cp.chars = 1;
            cp.isJoinerApos = true;
            return cp;
        }

        if (isAsciiWord(c)) {
            unsigned char lc = asciiLower(c);
            cp.type = CpType::Word;
            cp.b1 = (char)lc;
            cp.bytes = 1;
            cp.chars = 1;
            cp.isDigit = std::isdigit(lc);
            cp.isLetter = std::isalpha(lc);
            return cp;
        }

        cp.type = CpType::Other;
        cp.bytes = 1;
        return cp;
    }

    if (c == 0xE2 && i + 2 < s.size()) {
        unsigned char c2 = (unsigned char)s[i + 1];
        unsigned char c3 = (unsigned char)s[i + 2];
        if (c2 == 0x80 && (c3 == 0x93 || c3 == 0x94)) {
            cp.type = CpType::Joiner;
            cp.b1 = '-'; cp.bytes = 3; cp.chars = 1;
            cp.isJoinerHyphen = true;
            return cp;
        }
        if (c2 == 0x80 && c3 == 0x99) {
            cp.type = CpType::Joiner;
            cp.b1 = '\''; cp.bytes = 3; cp.chars = 1;
            cp.isJoinerApos = true;
            return cp;
        }
    }

    if ((c == 0xD0 || c == 0xD1) && i + 1 < s.size()) {
        unsigned char c2 = (unsigned char)s[i + 1];
        unsigned char n1, n2;
        if (normalizeCyr2(c, c2, n1, n2)) {
            cp.type = CpType::Word;
            cp.b1 = (char)n1; cp.b2 = (char)n2;
            cp.bytes = 2;
            cp.chars = 1;
            cp.isLetter = true;
            return cp;
        }
        cp.type = CpType::Other;
        cp.bytes = 2;
        return cp;
    }

    cp.type = CpType::Other;
    cp.bytes = 1;
    return cp;
}

std::vector<std::string> Tokenizer::tokenize(const std::string& utf8) {
    std::vector<std::string> out;
    out.reserve(256);

    std::string token;      token.reserve(64);
    std::string tokenFlat;  tokenFlat.reserve(64);
    std::vector<std::string> parts; parts.reserve(8);
    std::string part;       part.reserve(32);

    int tokenChars = 0;
    int partChars = 0;
    bool tooLong = false;
    bool hasAny = false;

    auto flushPart = [&]() {
        if (partChars >= 2 && partChars <= 50) parts.push_back(part);
        part.clear(); partChars = 0;
    };

    auto flushToken = [&]() {
        if (!hasAny) return;

        flushPart();

        if (!tooLong && tokenChars >= 2 && tokenChars <= 50) {
            out.push_back(token);
        }

        if (!tooLong && tokenFlat != token && (int)tokenFlat.size() > 0) {
            if (tokenFlat.size() >= 2) out.push_back(tokenFlat);
        }

        for (auto &p : parts) out.push_back(p);

        token.clear();
        tokenFlat.clear();
        parts.clear();
        hasAny = false;
        tokenChars = 0;
        tooLong = false;
    };

    for (size_t i = 0; i < utf8.size();) {
        if (isUrlStart(utf8, i)) {
            flushToken();
            i = skipUntilWhitespace(utf8, i);
            continue;
        }

        if (isEmailStartOrInside(utf8, i)) {
            flushToken();
            i = skipUntilWhitespace(utf8, i);
            continue;
        }

        Cp cp = readCp(utf8, i);

        if (cp.type == CpType::Word) {
            hasAny = true;

            if (!tooLong) {
                token.push_back(cp.b1);
                if (cp.bytes == 2) token.push_back(cp.b2);
            }

            if (!tooLong) {
                tokenFlat.push_back(cp.b1);
                if (cp.bytes == 2) tokenFlat.push_back(cp.b2);
            }

            part.push_back(cp.b1);
            if (cp.bytes == 2) part.push_back(cp.b2);

            tokenChars += 1;
            partChars += 1;
            if (tokenChars > 50) tooLong = true;

            i += cp.bytes;
            continue;
        }

        if (cp.type == CpType::Joiner) {
            size_t j = i + cp.bytes;
            bool nextIsWord = false;
            if (j < utf8.size()) {
                if (!isUrlStart(utf8, j) && !isEmailStartOrInside(utf8, j)) {
                    Cp nxt = readCp(utf8, j);
                    nextIsWord = (nxt.type == CpType::Word);
                }
            }

            if (hasAny && partChars > 0 && nextIsWord) {
                if (!tooLong) token.push_back(cp.b1);
                flushPart();
                tokenChars += 1;
                if (tokenChars > 50) tooLong = true;
            } else {
                flushToken();
            }

            i += cp.bytes;
            continue;
        }

        flushToken();
        i += cp.bytes;
    }

    flushToken();

    {
        std::unordered_set<std::string> seen;
        std::vector<std::string> uniq;
        uniq.reserve(out.size());
        for (auto &t : out) {
            if ((int)t.size() < 2) continue;
            if ((int)t.size() > 200) continue;
            if (seen.insert(t).second) uniq.push_back(std::move(t));
        }
        out = std::move(uniq);
    }

    return out;
}