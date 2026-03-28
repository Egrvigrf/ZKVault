#pragma once

#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct PasswordEntry {
    std::string name;
    std::string password;
    std::string note;
};

inline void to_json(json& j, const PasswordEntry& entry) {
    j = json{
        {"name", entry.name},
        {"password", entry.password},
        {"note", entry.note}
    };
}

inline void from_json(const json& j, PasswordEntry& entry) {
    j.at("name").get_to(entry.name);
    j.at("password").get_to(entry.password);
    j.at("note").get_to(entry.note);
}
