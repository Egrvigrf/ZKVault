#pragma once

#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct PasswordEntry {
    std::string name;
    std::string password;
    std::string note;
    std::string created_at;
    std::string updated_at;
};

inline void to_json(json& j, const PasswordEntry& entry) {
    j = json{
        {"name", entry.name},
        {"password", entry.password},
        {"note", entry.note},
        {"created_at", entry.created_at},
        {"updated_at", entry.updated_at}
    };
}

inline void from_json(const json& j, PasswordEntry& entry) {
    j.at("name").get_to(entry.name);
    j.at("password").get_to(entry.password);
    j.at("note").get_to(entry.note);
    if (j.contains("created_at")) j.at("created_at").get_to(entry.created_at);
    if (j.contains("updated_at")) j.at("updated_at").get_to(entry.updated_at);
}
