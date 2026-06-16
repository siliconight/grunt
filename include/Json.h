#pragma once
// Json.h — tiny, dependency-free JSON reader (objects, arrays, strings,
// numbers, bool, null). Enough to load voice.json + units.json without
// pulling an external library into the build. Not a general-purpose parser.
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <stdexcept>

namespace voc {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() : type_(Type::Null) {}

    static Json parse(const std::string& text); // throws std::runtime_error on malformed input

    Type type() const { return type_; }
    bool is_object() const { return type_ == Type::Object; }
    bool is_array()  const { return type_ == Type::Array; }

    bool        as_bool(bool def = false) const   { return type_ == Type::Bool ? bool_ : def; }
    double      as_number(double def = 0) const   { return type_ == Type::Number ? num_ : def; }
    std::string as_string(const std::string& def = "") const { return type_ == Type::String ? str_ : def; }

    bool has(const std::string& k) const { return obj_.count(k) > 0; }
    const Json& operator[](const std::string& k) const;
    const std::vector<Json>& items() const { return arr_; }

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0;
    std::string str_;
    std::vector<Json> arr_;
    std::map<std::string, Json> obj_;

    struct Parser;
    static const Json kNull;
};

} // namespace voc
