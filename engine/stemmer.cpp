#include "Stemmer.h"
#include <vector>

static constexpr char16_t RU_A = u'а';
static constexpr char16_t RU_YA = u'я';

std::string Stemmer::stem(const std::string& token) {
    return stemHyphenAposAware(token);
}

std::string Stemmer::stemHyphenAposAware(const std::string& token) {
    bool hasJoiner = false;
    for (unsigned char c : token) {
        if (c == '-' || c == '\'') { hasJoiner = true; break; }
    }
    if (!hasJoiner) return stemRu(token);

    std::string out;
    std::string part;
    char joiner = 0;

    auto flushPart = [&]() {
        if (!part.empty()) {
            out += stemRu(part);
            part.clear();
        }
    };

    for (size_t i = 0; i < token.size(); i++) {
        char c = token[i];
        if (c == '-' || c == '\'') {
            flushPart();
            out.push_back(c);
            joiner = c;
        } else {
            part.push_back(c);
        }
    }
    flushPart();

    return out;
}

bool Stemmer::hasCyrillic(const std::string& s) {
    for (unsigned char c : s) {
        if (c == 0xD0 || c == 0xD1) return true;
    }
    return false;
}

std::u16string Stemmer::toU16(const std::string& s) {
    std::u16string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {
            out.push_back((char16_t)c);
            i++;
            continue;
        }
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            unsigned char c2 = (unsigned char)s[i+1];
            if ((c2 & 0xC0) == 0x80) {
                char16_t cp = (char16_t)(((c & 0x1F) << 6) | (c2 & 0x3F));
                out.push_back(cp);
                i += 2;
                continue;
            }
        }
        i++;
    }
    return out;
}

std::string Stemmer::fromU16(const std::u16string& s) {
    std::string out;
    out.reserve(s.size() * 2);

    for (char16_t cp : s) {
        if (cp < 0x80) {
            out.push_back((char)cp);
        } else {
            unsigned char b1 = (unsigned char)(0xC0 | (cp >> 6));
            unsigned char b2 = (unsigned char)(0x80 | (cp & 0x3F));
            out.push_back((char)b1);
            out.push_back((char)b2);
        }
    }
    return out;
}

bool Stemmer::isVowel(char16_t ch) {
    switch (ch) {
        case u'а': case u'е': case u'и': case u'о': case u'у':
        case u'ы': case u'э': case u'ю': case u'я':
            return true;
        default:
            return false;
    }
}

size_t Stemmer::findRV(const std::u16string& w) {
    for (size_t i = 0; i < w.size(); i++) {
        if (isVowel(w[i])) return i + 1;
    }
    return w.size();
}

size_t Stemmer::findR1(const std::u16string& w, size_t start) {
    bool seenVowel = false;
    for (size_t i = start; i < w.size(); i++) {
        if (isVowel(w[i])) seenVowel = true;
        else if (seenVowel) return i + 1;
    }
    return w.size();
}

size_t Stemmer::findR2(const std::u16string& w) {
    size_t r1 = findR1(w, 0);
    return findR1(w, r1);
}

bool Stemmer::endsWith(const std::u16string& w, const std::u16string& suf) {
    if (w.size() < suf.size()) return false;
    for (size_t i = 0; i < suf.size(); i++) {
        if (w[w.size() - suf.size() + i] != suf[i]) return false;
    }
    return true;
}

bool Stemmer::removeSuffixInRegion(std::u16string& w, size_t region, const std::u16string& suf) {
    if (w.size() < suf.size()) return false;
    if (w.size() - suf.size() < region) return false;
    if (!endsWith(w, suf)) return false;
    w.erase(w.size() - suf.size());
    return true;
}

bool Stemmer::removeAnySuffixInRegion(std::u16string& w, size_t region, const std::u16string* sufs, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (removeSuffixInRegion(w, region, sufs[i])) return true;
    }
    return false;
}

bool Stemmer::removeIfPrecededByAY(std::u16string& w, size_t region, const std::u16string& suf) {
    if (!endsWith(w, suf)) return false;
    if (w.size() - suf.size() < region) return false;
    if (w.size() <= suf.size()) return false;
    char16_t prev = w[w.size() - suf.size() - 1];
    if (prev != RU_A && prev != RU_YA) return false;
    w.erase(w.size() - suf.size());
    return true;
}

std::string Stemmer::stemRu(const std::string& token) {
    if (!hasCyrillic(token)) return token;

    std::u16string w = toU16(token);
    if (w.size() < 2) return token;

    size_t rv = findRV(w);
    size_t r2 = findR2(w);

    if (rv >= w.size()) return token;

    static const std::u16string PG1[] = { u"ив", u"ивши", u"ившись", u"ыв", u"ывши", u"ывшись" };
    static const std::u16string PG2[] = { u"в", u"вши", u"вшись" };

    bool removed = removeAnySuffixInRegion(w, rv, PG1, sizeof(PG1)/sizeof(PG1[0]));
    if (!removed) {
        for (const auto& suf : PG2) {
            if (removeIfPrecededByAY(w, rv, suf)) { removed = true; break; }
        }
    }

    if (!removed) {
        static const std::u16string REF[] = { u"ся", u"сь" };
        removeAnySuffixInRegion(w, rv, REF, sizeof(REF)/sizeof(REF[0]));

        static const std::u16string ADJ[] = {
            u"ее",u"ие",u"ое",u"ые",u"ими",u"ыми",u"ей",u"ий",u"ой",u"ый",
            u"ем",u"им",u"ым",u"его",u"ого",u"ему",u"ому",u"их",u"ых",u"ую",u"юю",u"ая",u"яя",u"ою",u"ею"
        };

        static const std::u16string PART1[] = { u"ем", u"нн", u"вш", u"ющ", u"щ" };
        static const std::u16string PART2[] = { u"ивш", u"ывш", u"ующ" };

        bool adjRemoved = removeAnySuffixInRegion(w, rv, ADJ, sizeof(ADJ)/sizeof(ADJ[0]));
        if (adjRemoved) {
            bool partRemoved = removeAnySuffixInRegion(w, rv, PART2, sizeof(PART2)/sizeof(PART2[0]));
            if (!partRemoved) {
                for (const auto& suf : PART1) {
                    if (removeIfPrecededByAY(w, rv, suf)) break;
                }
            }
        } else {
            static const std::u16string VERB1[] = {
                u"ла",u"на",u"ете",u"йте",u"ли",u"й",u"л",u"ем",u"н",u"ло",u"но",u"ет",u"ют",u"ны",u"ть",u"ешь",u"нно"
            };
            static const std::u16string VERB2[] = {
                u"ила",u"ыла",u"ена",u"ейте",u"уйте",u"ите",u"или",u"ыли",u"ей",u"уй",u"ил",u"ыл",u"им",u"ым",u"ен",
                u"ило",u"ыло",u"ено",u"ят",u"ует",u"уют",u"ит",u"ыт",u"ены",u"ить",u"ыть",u"ишь",u"ую",u"ю"
            };

            bool verbRemoved = removeAnySuffixInRegion(w, rv, VERB2, sizeof(VERB2)/sizeof(VERB2[0]));
            if (!verbRemoved) {
                for (const auto& suf : VERB1) {
                    if (removeIfPrecededByAY(w, rv, suf)) { verbRemoved = true; break; }
                }
            }

            if (!verbRemoved) {
                static const std::u16string NOUN[] = {
                    u"а",u"ев",u"ов",u"ие",u"ье",u"е",u"иями",u"ями",u"ами",u"еи",u"ии",u"и",u"ией",u"ей",u"ой",u"ий",u"й",
                    u"иям",u"ям",u"ием",u"ем",u"ам",u"ом",u"о",u"у",u"ах",u"иях",u"ях",u"ы",u"ь",u"ию",u"ью",u"ю",u"ия",u"я"
                };
                removeAnySuffixInRegion(w, rv, NOUN, sizeof(NOUN)/sizeof(NOUN[0]));
            }
        }
    }

    removeSuffixInRegion(w, rv, u"и");

    removeSuffixInRegion(w, r2, u"ость");

    static const std::u16string SUPER[] = { u"ейше", u"ейш" };
    if (removeAnySuffixInRegion(w, rv, SUPER, sizeof(SUPER)/sizeof(SUPER[0]))) {
        if (endsWith(w, u"нн")) w.erase(w.size() - 1);
    }

    if (!removeSuffixInRegion(w, rv, u"ь")) {
        if (endsWith(w, u"нн")) w.erase(w.size() - 1);
    }

    return fromU16(w);
}