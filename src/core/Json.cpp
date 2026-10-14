#include "Json.h"

#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace lumen::json {

// Recursive descent: one function per kind of value.
class Parser {
public:
    explicit Parser(const std::string& t) : text_(t) {}

    Value parseDocument() {
        Value v = parseValue();
        skipSpace();
        if (pos_ != text_.size()) fail("unexpected text after the end");
        return v;
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(const std::string& what) const {
        int line = 1;
        for (std::size_t i = 0; i < pos_ && i < text_.size(); ++i)
            if (text_[i] == '\n') ++line;
        throw std::runtime_error("not valid JSON, line " + std::to_string(line) + ": " + what);
    }

    void skipSpace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }

    char peek() {
        skipSpace();
        if (pos_ >= text_.size()) fail("the file ends too early");
        return text_[pos_];
    }

    void expect(char c) {
        if (peek() != c) fail(std::string("expected '") + c + "'");
        ++pos_;
    }

    Value parseValue() {
        const char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') {
            Value v;
            v.type_ = Value::Type::String;
            v.string_ = parseString();
            return v;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber();
        if (text_.compare(pos_, 4, "true") == 0)  { pos_ += 4; Value v; v.type_ = Value::Type::Bool; v.boolean_ = true; return v; }
        if (text_.compare(pos_, 5, "false") == 0) { pos_ += 5; Value v; v.type_ = Value::Type::Bool; return v; }
        if (text_.compare(pos_, 4, "null") == 0)  { pos_ += 4; return Value{}; }
        fail("unexpected character");
    }

    Value parseObject() {
        Value v;
        v.type_ = Value::Type::Object;
        expect('{');
        if (peek() == '}') { ++pos_; return v; }
        for (;;) {
            if (peek() != '"') fail("expected a key in quotes");
            const std::string key = parseString();
            expect(':');
            v.object_[key] = parseValue();
            if (peek() == ',') { ++pos_; continue; }
            expect('}');
            return v;
        }
    }

    Value parseArray() {
        Value v;
        v.type_ = Value::Type::Array;
        expect('[');
        if (peek() == ']') { ++pos_; return v; }
        for (;;) {
            v.array_.push_back(parseValue());
            if (peek() == ',') { ++pos_; continue; }
            expect(']');
            return v;
        }
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (pos_ < text_.size() && text_[pos_] != '"') {
            char c = text_[pos_++];
            if (c == '\\' && pos_ < text_.size()) {
                const char e = text_[pos_++];
                switch (e) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'u': pos_ += 4; c = '?'; break;   // not needed by the plan
                    default:  c = e; break;
                }
            }
            out += c;
        }
        if (pos_ >= text_.size()) fail("a string is never closed");
        ++pos_;
        return out;
    }

    Value parseNumber() {
        const char* start = text_.c_str() + pos_;
        char* end = nullptr;
        const double d = std::strtod(start, &end);
        if (end == start) fail("bad number");
        pos_ += static_cast<std::size_t>(end - start);
        Value v;
        v.type_ = Value::Type::Number;
        v.number_ = d;
        return v;
    }
};

Value Value::parse(const std::string& text) { return Parser(text).parseDocument(); }

double Value::number() const {
    if (type_ != Type::Number) throw std::runtime_error("expected a number");
    return number_;
}

const std::string& Value::string() const {
    if (type_ != Type::String) throw std::runtime_error("expected a string");
    return string_;
}

const std::vector<Value>& Value::array() const {
    if (type_ != Type::Array) throw std::runtime_error("expected a list");
    return array_;
}

std::size_t Value::size() const { return array().size(); }

const Value& Value::operator[](std::size_t i) const {
    const std::vector<Value>& a = array();
    if (i >= a.size()) throw std::runtime_error("a list is too short");
    return a[i];
}

const Value& Value::at(const std::string& key) const {
    if (type_ != Type::Object) throw std::runtime_error("expected an object with '" + key + "'");
    const auto it = object_.find(key);
    if (it == object_.end()) throw std::runtime_error("missing field '" + key + "'");
    return it->second;
}

bool Value::contains(const std::string& key) const {
    return type_ == Type::Object && object_.count(key) > 0;
}

double Value::numberOr(const std::string& key, double fallback) const {
    return contains(key) ? at(key).number() : fallback;
}

} // namespace lumen::json
