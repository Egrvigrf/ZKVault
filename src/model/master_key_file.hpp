#pragma once

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
