// Json.h - a small JSON reader, enough for assets/plan.json. Owner: Erykah.
//
// Written in plain C++ so the project needs no libraries at all.

#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lumen::json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type() const { return type_; }
    bool isArray()  const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    // These throw std::runtime_error with a readable message on a wrong type.
    double             number() const;
    const std::string& string() const;
    const std::vector<Value>& array() const;

    std::size_t  size() const;                       // array length
    const Value& operator[](std::size_t i) const;    // array element
    const Value& at(const std::string& key) const;   // object member, must exist
    bool         contains(const std::string& key) const;
    double       numberOr(const std::string& key, double fallback) const;

    // Parses text. Throws std::runtime_error with the line number on bad JSON.
    static Value parse(const std::string& text);

private:
    friend class Parser;
    Type type_ = Type::Null;
    bool boolean_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Value> array_;
    std::map<std::string, Value> object_;
};

} // namespace lumen::json
