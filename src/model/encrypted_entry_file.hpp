#pragma once

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct EncryptedEntryFile {
    int version;
    std::string data_iv;
    std::string ciphertext;
    std::string auth_tag;
};

inline void to_json(json& j, const EncryptedEntryFile& file) {
    j = json{
        {"version", file.version},
        {"data_iv", file.data_iv},
        {"ciphertext", file.ciphertext},
        {"auth_tag", file.auth_tag}
    };
}

inline void from_json(const json& j, EncryptedEntryFile& file) {
    j.at("version").get_to(file.version);
    j.at("data_iv").get_to(file.data_iv);
    j.at("ciphertext").get_to(file.ciphertext);
    j.at("auth_tag").get_to(file.auth_tag);
}

inline void ValidateEncryptedEntryFile(const EncryptedEntryFile& file) {
    if (file.version != 1) {
        throw std::runtime_error("unsupported encrypted entry version");
    }

    if (file.data_iv.empty()) {
        throw std::runtime_error("encrypted entry data_iv must not be empty");
    }

    if (file.auth_tag.empty()) {
        throw std::runtime_error("encrypted entry auth_tag must not be empty");
    }
}
