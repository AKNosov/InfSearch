#include "b_srch.h"
#include "Tokenizer.h"
#include "Stemmer.h"
#include <algorithm>
#include <cctype>

bool BooleanSearch::isOp(TokType t){ return t==TokType::AND||t==TokType::OR||t==TokType::NOT; }
int  BooleanSearch::prec(TokType t){ return (t==TokType::NOT)?3:(t==TokType::AND)?2:(t==TokType::OR)?1:0; }

std::vector<int> BooleanSearch::opAnd(const std::vector<int>& a, const std::vector<int>& b){
    std::vector<int> out; out.reserve(std::min(a.size(), b.size()));
    size_t i=0,j=0;
    while(i<a.size()&&j<b.size()){
        if(a[i]==b[j]){ out.push_back(a[i]); i++; j++; }
        else if(a[i]<b[j]) i++; else j++;
    }
    return out;
}
std::vector<int> BooleanSearch::opOr(const std::vector<int>& a, const std::vector<int>& b){
    std::vector<int> out; out.reserve(a.size()+b.size());
    size_t i=0,j=0;
    while(i<a.size()||j<b.size()){
        if(j==b.size()||(i<a.size()&&a[i]<b[j])) out.push_back(a[i++]);
        else if(i==a.size()||b[j]<a[i]) out.push_back(b[j++]);
        else { out.push_back(a[i]); i++; j++; }
    }
    return out;
}
std::vector<int> BooleanSearch::opNot(const std::vector<int>& u, const std::vector<int>& b){
    std::vector<int> out; out.reserve(u.size());
    size_t i=0,j=0;
    while(i<u.size()){
        if(j==b.size()||u[i]<b[j]) out.push_back(u[i++]);
        else if(u[i]==b[j]){ i++; j++; }
        else j++;
    }
    return out;
}

static bool isAsciiWord(const std::string& s){
    if(s.empty()) return false;
    for(unsigned char c: s) if(!(c<128 && std::isalpha(c))) return false;
    return true;
}
static std::string upperAscii(std::string s){
    for(char& c: s) if((unsigned char)c<128) c=(char)std::toupper((unsigned char)c);
    return s;
}

std::vector<BooleanSearch::Tok> BooleanSearch::lex(const std::string& q) const {
    std::vector<Tok> raw;
    std::string buf;

    auto flush = [&](){
        if(buf.empty()) return;
        if(isAsciiWord(buf)){
            auto up = upperAscii(buf);
            if(up=="AND"){ raw.push_back({TokType::AND,{}}); buf.clear(); return; }
            if(up=="OR"){  raw.push_back({TokType::OR,{}});  buf.clear(); return; }
            if(up=="NOT"){ raw.push_back({TokType::NOT,{}}); buf.clear(); return; }
        }
        auto toks = Tokenizer::tokenize(buf);
        for(auto& t: toks){
            auto term = Stemmer::stem(t);
            if(!term.empty()) raw.push_back({TokType::TERM, term});
        }
        buf.clear();
    };

    for(char c: q){
        if(c=='('){ flush(); raw.push_back({TokType::LPAREN,{}}); }
        else if(c==')'){ flush(); raw.push_back({TokType::RPAREN,{}}); }
        else if(std::isspace((unsigned char)c)) flush();
        else buf.push_back(c);
    }
    flush();

    std::vector<Tok> norm;
    for(size_t i=0;i<raw.size();i++){
        norm.push_back(raw[i]);
        if(i+1<raw.size()){
            auto a=raw[i].type, b=raw[i+1].type;
            bool left  = (a==TokType::TERM || a==TokType::RPAREN);
            bool right = (b==TokType::TERM || b==TokType::LPAREN || b==TokType::NOT);
            if(left && right) norm.push_back({TokType::AND,{}});
        }
    }
    return norm;
}

std::vector<BooleanSearch::Tok> BooleanSearch::toRpn(const std::vector<Tok>& toks) const {
    std::vector<Tok> out, st;
    for(auto& tk: toks){
        if(tk.type==TokType::TERM) out.push_back(tk);
        else if(isOp(tk.type)){
            while(!st.empty() && isOp(st.back().type) &&
                  (prec(st.back().type)>prec(tk.type) ||
                   (prec(st.back().type)==prec(tk.type) && tk.type!=TokType::NOT))){
                out.push_back(st.back()); st.pop_back();
            }
            st.push_back(tk);
        } else if(tk.type==TokType::LPAREN) st.push_back(tk);
        else if(tk.type==TokType::RPAREN){
            while(!st.empty() && st.back().type!=TokType::LPAREN){ out.push_back(st.back()); st.pop_back(); }
            if(!st.empty() && st.back().type==TokType::LPAREN) st.pop_back();
        }
    }
    while(!st.empty()){ out.push_back(st.back()); st.pop_back(); }
    return out;
}

std::vector<int> BooleanSearch::evalRpn(const std::vector<Tok>& rpn) const {
    std::vector<std::vector<int>> st;
    for(auto& tk: rpn){
        if(tk.type==TokType::TERM){
            st.push_back(idx_.postings(tk.val));
        } else if(tk.type==TokType::NOT){
            auto a = st.empty()?std::vector<int>{}:std::move(st.back());
            if(!st.empty()) st.pop_back();
            st.push_back(opNot(idx_.allDocs(), a));
        } else if(tk.type==TokType::AND){
            if(st.size()<2){ st.push_back({}); continue; }
            auto b=std::move(st.back()); st.pop_back();
            auto a=std::move(st.back()); st.pop_back();
            st.push_back(opAnd(a,b));
        } else if(tk.type==TokType::OR){
            if(st.size()<2){ st.push_back({}); continue; }
            auto b=std::move(st.back()); st.pop_back();
            auto a=std::move(st.back()); st.pop_back();
            st.push_back(opOr(a,b));
        }
    }
    return st.empty()?std::vector<int>{}:std::move(st.back());
}

std::vector<int> BooleanSearch::search(const std::string& query) const {
    auto toks = lex(query);
    auto rpn  = toRpn(toks);
    return evalRpn(rpn);
}