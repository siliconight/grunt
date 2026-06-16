#include "Json.h"
#include <cctype>

namespace voc {

const Json Json::kNull{};

const Json& Json::operator[](const std::string& k) const {
    auto it = obj_.find(k);
    return it == obj_.end() ? kNull : it->second;
}

struct Json::Parser {
    const std::string& s;
    size_t i = 0;
    explicit Parser(const std::string& src) : s(src) {}

    [[noreturn]] void fail(const std::string& m) {
        throw std::runtime_error("JSON parse error at " + std::to_string(i) + ": " + m);
    }
    void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }
    char peek() { return i < s.size() ? s[i] : '\0'; }

    Json value() {
        ws();
        char c = peek();
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') { Json j; j.type_ = Type::String; j.str_ = str(); return j; }
        if (c == 't' || c == 'f') return boolean();
        if (c == 'n') { lit("null"); return Json{}; }
        return number();
    }

    void lit(const char* w) { for (const char* p = w; *p; ++p) { if (peek() != *p) fail("bad literal"); ++i; } }

    Json boolean() {
        Json j; j.type_ = Type::Bool;
        if (peek() == 't') { lit("true"); j.bool_ = true; }
        else { lit("false"); j.bool_ = false; }
        return j;
    }

    Json number() {
        size_t start = i;
        if (peek() == '-') ++i;
        while (std::isdigit((unsigned char)peek())) ++i;
        if (peek() == '.') { ++i; while (std::isdigit((unsigned char)peek())) ++i; }
        if (peek() == 'e' || peek() == 'E') {
            ++i; if (peek() == '+' || peek() == '-') ++i;
            while (std::isdigit((unsigned char)peek())) ++i;
        }
        if (i == start) fail("expected number");
        Json j; j.type_ = Type::Number; j.num_ = std::stod(s.substr(start, i - start));
        return j;
    }

    std::string str() {
        if (peek() != '"') fail("expected string");
        ++i;
        std::string out;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return out;
            if (c == '\\') {
                if (i >= s.size()) fail("bad escape");
                char e = s[i++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        if (i + 4 > s.size()) fail("bad \\u");
                        // minimal: only handle ASCII range
                        int code = std::stoi(s.substr(i, 4), nullptr, 16);
                        i += 4;
                        if (code < 0x80) out += (char)code;
                        else out += '?';
                        break;
                    }
                    default: fail("unknown escape");
                }
            } else {
                out += c;
            }
        }
        fail("unterminated string");
    }

    Json array() {
        Json j; j.type_ = Type::Array;
        ++i; ws();
        if (peek() == ']') { ++i; return j; }
        while (true) {
            j.arr_.push_back(value());
            ws();
            char c = peek();
            if (c == ',') { ++i; continue; }
            if (c == ']') { ++i; break; }
            fail("expected , or ]");
        }
        return j;
    }

    Json object() {
        Json j; j.type_ = Type::Object;
        ++i; ws();
        if (peek() == '}') { ++i; return j; }
        while (true) {
            ws();
            std::string key = str();
            ws();
            if (peek() != ':') fail("expected :");
            ++i;
            j.obj_[key] = value();
            ws();
            char c = peek();
            if (c == ',') { ++i; continue; }
            if (c == '}') { ++i; break; }
            fail("expected , or }");
        }
        return j;
    }
};

Json Json::parse(const std::string& text) {
    Parser p(text);
    Json j = p.value();
    p.ws();
    return j;
}

} // namespace voc
