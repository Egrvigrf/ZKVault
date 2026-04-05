#pragma once

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct MasterKeyFile {
    int version;
    std::string kdf;
    std::string salt;
    std::string wrap_iv;
    std::string encrypted_dek;
    std::string auth_tag;
};

inline void to_json(json& j, const MasterKeyFile& file) {
    j = json{
        {"version", file.version},
        {"kdf", file.kdf},
        {"salt", file.salt},
        {"wrap_iv", file.wrap_iv},
        {"encrypted_dek", file.encrypted_dek},
        {"auth_tag", file.auth_tag}
    };
}

inline void from_json(const json& j, MasterKeyFile& file) {
    j.at("version").get_to(file.version);
    j.at("kdf").get_to(file.kdf);
    j.at("salt").get_to(file.salt);
    j.at("wrap_iv").get_to(file.wrap_iv);
    j.at("encrypted_dek").get_to(file.encrypted_dek);
    j.at("auth_tag").get_to(file.auth_tag);
}

inline void ValidateMasterKeyFile(const MasterKeyFile& file) {
    if (file.version != 1) {
        throw std::runtime_error("unsupported .zkv_master version");
    }

    if (file.kdf != "scrypt") {
        throw std::runtime_error("unsupported .zkv_master kdf");
    }

    if (file.salt.empty()) {
        throw std::runtime_error(".zkv_master salt must not be empty");
    }

    if (file.wrap_iv.empty()) {
        throw std::runtime_error(".zkv_master wrap_iv must not be empty");
    }

    if (file.encrypted_dek.empty()) {
        throw std::runtime_error(".zkv_master encrypted_dek must not be empty");
    }

    if (file.auth_tag.empty()) {
        throw std::runtime_error(".zkv_master auth_tag must not be empty");
    }
}
