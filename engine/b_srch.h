#pragma once
#include <string>
#include <vector>
#include "b_idx.h"

class BooleanSearch {
public:
    explicit BooleanSearch(const BooleanIndex& idx) : idx_(idx) {}
    std::vector<int> search(const std::string& query) const;

private:
    const BooleanIndex& idx_;

    enum class TokType { TERM, AND, OR, NOT, LPAREN, RPAREN };
    struct Tok { TokType type; std::string val; };

    std::vector<Tok> lex(const std::string& q) const;
    std::vector<Tok> toRpn(const std::vector<Tok>& toks) const;
    std::vector<int> evalRpn(const std::vector<Tok>& rpn) const;

    static bool isOp(TokType t);
    static int prec(TokType t);

    static std::vector<int> opAnd(const std::vector<int>& a, const std::vector<int>& b);
    static std::vector<int> opOr (const std::vector<int>& a, const std::vector<int>& b);
    static std::vector<int> opNot(const std::vector<int>& universe, const std::vector<int>& b);
};