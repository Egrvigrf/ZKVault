#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

struct PrivatePostMetadata {
    std::string slug;
    std::string title;
    std::string excerpt;
    std::string published_at;
};

struct PrivatePostDocument {
    PrivatePostMetadata metadata;
    std::string markdown;
};

struct EncryptedPrivatePostBundle {
    int version;
    std::string payload_format;
    std::string cipher;
    std::string kdf;
    std::string salt;
    std::string data_iv;
    std::string ciphertext;
    std::string auth_tag;
    PrivatePostMetadata metadata;
};

void ValidatePrivatePostSlug(std::string_view slug);

void ValidatePrivatePostMetadata(const PrivatePostMetadata& metadata);

void ValidatePrivatePostDocument(const PrivatePostDocument& document);

void ValidateEncryptedPrivatePostBundle(
    const EncryptedPrivatePostBundle& bundle);

EncryptedPrivatePostBundle EncryptPrivatePostDocument(
    const PrivatePostDocument& document,
    const std::string& access_password);

PrivatePostDocument DecryptPrivatePostDocument(
    const EncryptedPrivatePostBundle& bundle,
    const std::string& access_password);

inline void to_json(nlohmann::json& j, const PrivatePostMetadata& metadata) {
    j = nlohmann::json{
        {"slug", metadata.slug},
        {"title", metadata.title},
        {"excerpt", metadata.excerpt},
        {"published_at", metadata.published_at}
    };
}

inline void from_json(const nlohmann::json& j, PrivatePostMetadata& metadata) {
    j.at("slug").get_to(metadata.slug);
    j.at("title").get_to(metadata.title);
    if (j.contains("excerpt")) j.at("excerpt").get_to(metadata.excerpt);
    if (j.contains("published_at")) {
        j.at("published_at").get_to(metadata.published_at);
    }
}

inline void to_json(nlohmann::json& j, const PrivatePostDocument& document) {
    j = nlohmann::json{
        {"metadata", document.metadata},
        {"markdown", document.markdown}
    };
}

inline void from_json(const nlohmann::json& j, PrivatePostDocument& document) {
    j.at("metadata").get_to(document.metadata);
    j.at("markdown").get_to(document.markdown);
}

inline void to_json(
    nlohmann::json& j,
    const EncryptedPrivatePostBundle& bundle) {
    j = nlohmann::json{
        {"version", bundle.version},
        {"payload_format", bundle.payload_format},
        {"cipher", bundle.cipher},
        {"kdf", bundle.kdf},
        {"salt", bundle.salt},
        {"data_iv", bundle.data_iv},
        {"ciphertext", bundle.ciphertext},
        {"auth_tag", bundle.auth_tag},
        {"metadata", bundle.metadata}
    };
}

inline void from_json(
    const nlohmann::json& j,
    EncryptedPrivatePostBundle& bundle) {
    j.at("version").get_to(bundle.version);
    j.at("payload_format").get_to(bundle.payload_format);
    j.at("cipher").get_to(bundle.cipher);
    j.at("kdf").get_to(bundle.kdf);
    j.at("salt").get_to(bundle.salt);
    j.at("data_iv").get_to(bundle.data_iv);
    j.at("ciphertext").get_to(bundle.ciphertext);
    j.at("auth_tag").get_to(bundle.auth_tag);
    j.at("metadata").get_to(bundle.metadata);
}
