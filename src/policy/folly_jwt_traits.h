#pragma once

// jwt-cpp traits adapter using folly::dynamic as the JSON backend.
// Avoids adding picojson/nlohmann as separate dependencies.

#include <jwt-cpp/jwt.h>

#include <folly/dynamic.h>
#include <folly/json.h>

#include <map>
#include <stdexcept>
#include <vector>

namespace jwt {
namespace traits {

struct folly_dynamic {
    using json          = folly::dynamic;
    using value_type    = folly::dynamic;
    using object_type   = std::map<std::string, folly::dynamic>;
    using array_type    = std::vector<folly::dynamic>;
    using string_type   = std::string;
    using number_type   = double;
    using integer_type  = int64_t;
    using boolean_type  = bool;

    static jwt::json::type get_type(const folly::dynamic& val) {
        using jwt::json::type;
        switch (val.type()) {
        case folly::dynamic::Type::BOOL:   return type::boolean;
        case folly::dynamic::Type::INT64:  return type::integer;
        case folly::dynamic::Type::DOUBLE: return type::number;
        case folly::dynamic::Type::STRING: return type::string;
        case folly::dynamic::Type::ARRAY:  return type::array;
        case folly::dynamic::Type::OBJECT: return type::object;
        default: throw std::logic_error("unsupported folly::dynamic type in JWT");
        }
    }

    static object_type as_object(const folly::dynamic& val) {
        if (!val.isObject()) throw std::bad_cast();
        object_type result;
        for (const auto& [k, v] : val.items()) {
            result[k.asString()] = v;
        }
        return result;
    }

    static std::string as_string(const folly::dynamic& val) {
        if (!val.isString()) throw std::bad_cast();
        return val.asString();
    }

    static array_type as_array(const folly::dynamic& val) {
        if (!val.isArray()) throw std::bad_cast();
        return array_type(val.begin(), val.end());
    }

    static int64_t as_integer(const folly::dynamic& val) {
        if (!val.isInt()) throw std::bad_cast();
        return val.asInt();
    }

    static bool as_boolean(const folly::dynamic& val) {
        if (!val.isBool()) throw std::bad_cast();
        return val.asBool();
    }

    static double as_number(const folly::dynamic& val) {
        if (!val.isDouble()) throw std::bad_cast();
        return val.asDouble();
    }

    static bool parse(folly::dynamic& val, std::string str) {
        try {
            val = folly::parseJson(str);
            return true;
        } catch (...) {
            return false;
        }
    }

    static std::string serialize(const folly::dynamic& val) {
        return folly::toJson(val);
    }
};

} // namespace traits
} // namespace jwt
