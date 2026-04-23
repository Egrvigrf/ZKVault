#include "content/private_post.hpp"

#include <cctype>
#include <vector>

#include "crypto/aead.hpp"
#include "crypto/hex.hpp"
#include "crypto/kdf.hpp"
#include "crypto/random.hpp"
#include "crypto/secure_memory.hpp"

namespace {

struct PrivatePostSecretPayload {
    std::string markdown;
};

void Cleanse(PrivatePostSecretPayload& payload) {
    ::Cleanse(payload.markdown);
}

void ValidatePrivatePostAccessPassword(const std::string& access_password) {
    if (access_password.empty()) {
        throw std::runtime_error("private post access password must not be empty");
    }
}

void ValidateSlugSegment(std::string_view segment) {
    if (segment.empty()) {
        throw std::runtime_error("private post slug must not contain empty path segments");
    }

    if (segment == "." || segment == "..") {
        throw std::runtime_error("private post slug must not contain '.' or '..' segments");
    }

    for (const unsigned char ch : segment) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.') {
            continue;
        }

        throw std::runtime_error(
            "private post slug may only contain letters, digits, '.', '-', '_' and '/'");
    }
}

void ValidateRequiredHexField(
    std::string_view value,
    std::string_view field_name) {
    if (value.empty()) {
        throw std::runtime_error(
            "encrypted private post " + std::string(field_name) + " must not be empty");
    }
}

inline void to_json(nlohmann::json& j, const PrivatePostSecretPayload& payload) {
    j = nlohmann::json{{"markdown", payload.markdown}};
}

inline void from_json(
    const nlohmann::json& j,
    PrivatePostSecretPayload& payload) {
    j.at("markdown").get_to(payload.markdown);
}

}  // namespace

void ValidatePrivatePostSlug(std::string_view slug) {
    if (slug.empty()) {
        throw std::runtime_error("private post slug must not be empty");
    }

    if (slug.front() == '/' || slug.back() == '/') {
        throw std::runtime_error(
            "private post slug must not start or end with '/'");
    }

    std::size_t segment_start = 0;
    while (segment_start < slug.size()) {
        const std::size_t separator = slug.find('/', segment_start);
        if (separator == std::string_view::npos) {
            ValidateSlugSegment(slug.substr(segment_start));
            return;
        }

        ValidateSlugSegment(slug.substr(segment_start, separator - segment_start));
        segment_start = separator + 1;
    }
}

void ValidatePrivatePostMetadata(const PrivatePostMetadata& metadata) {
    ValidatePrivatePostSlug(metadata.slug);
    if (metadata.title.empty()) {
        throw std::runtime_error("private post title must not be empty");
    }
}

void ValidatePrivatePostDocument(const PrivatePostDocument& document) {
    ValidatePrivatePostMetadata(document.metadata);
}

void ValidateEncryptedPrivatePostBundle(
    const EncryptedPrivatePostBundle& bundle) {
    if (bundle.version != 1) {
        throw std::runtime_error("unsupported encrypted private post version");
    }

    if (bundle.payload_format != "markdown") {
        throw std::runtime_error("unsupported encrypted private post payload format");
    }

    if (bundle.cipher != "aes-256-gcm") {
        throw std::runtime_error("unsupported encrypted private post cipher");
    }

    if (bundle.kdf != "scrypt") {
        throw std::runtime_error("unsupported encrypted private post kdf");
    }

    ValidateRequiredHexField(bundle.salt, "salt");
    ValidateRequiredHexField(bundle.data_iv, "data_iv");
    ValidateRequiredHexField(bundle.ciphertext, "ciphertext");
    ValidateRequiredHexField(bundle.auth_tag, "auth_tag");
    ValidatePrivatePostMetadata(bundle.metadata);
}

EncryptedPrivatePostBundle EncryptPrivatePostDocument(
    const PrivatePostDocument& document,
    const std::string& access_password) {
    ValidatePrivatePostDocument(document);
    ValidatePrivatePostAccessPassword(access_password);

    std::vector<unsigned char> salt = GenerateRandomBytes(16);
    auto salt_guard = MakeScopedCleanse(salt);
    std::vector<unsigned char> key =
        DeriveKeyScrypt(access_password, salt, 32);
    auto key_guard = MakeScopedCleanse(key);

    PrivatePostSecretPayload payload{document.markdown};
    auto payload_guard = MakeScopedCleanse(payload);
    std::string plaintext = nlohmann::json(payload).dump();
    auto plaintext_guard = MakeScopedCleanse(plaintext);
    std::vector<unsigned char> plaintext_bytes(plaintext.begin(), plaintext.end());
    auto plaintext_bytes_guard = MakeScopedCleanse(plaintext_bytes);
    const AeadCiphertext encrypted = EncryptAes256Gcm(key, plaintext_bytes);

    return EncryptedPrivatePostBundle{
        1,
        "markdown",
        "aes-256-gcm",
        "scrypt",
        BytesToHex(salt),
        BytesToHex(encrypted.iv),
        BytesToHex(encrypted.ciphertext),
        BytesToHex(encrypted.auth_tag),
        document.metadata
    };
}

PrivatePostDocument DecryptPrivatePostDocument(
    const EncryptedPrivatePostBundle& bundle,
    const std::string& access_password) {
    ValidateEncryptedPrivatePostBundle(bundle);
    ValidatePrivatePostAccessPassword(access_password);

    std::vector<unsigned char> salt = HexToBytes(bundle.salt);
    auto salt_guard = MakeScopedCleanse(salt);
    std::vector<unsigned char> key =
        DeriveKeyScrypt(access_password, salt, 32);
    auto key_guard = MakeScopedCleanse(key);
    std::vector<unsigned char> iv = HexToBytes(bundle.data_iv);
    auto iv_guard = MakeScopedCleanse(iv);
    std::vector<unsigned char> ciphertext = HexToBytes(bundle.ciphertext);
    auto ciphertext_guard = MakeScopedCleanse(ciphertext);
    std::vector<unsigned char> auth_tag = HexToBytes(bundle.auth_tag);
    auto auth_tag_guard = MakeScopedCleanse(auth_tag);
    std::vector<unsigned char> plaintext_bytes =
        DecryptAes256Gcm(key, iv, ciphertext, auth_tag);
    auto plaintext_bytes_guard = MakeScopedCleanse(plaintext_bytes);
    std::string plaintext(plaintext_bytes.begin(), plaintext_bytes.end());
    auto plaintext_guard = MakeScopedCleanse(plaintext);

    try {
        PrivatePostSecretPayload payload =
            nlohmann::json::parse(plaintext).get<PrivatePostSecretPayload>();
        auto payload_guard = MakeScopedCleanse(payload);
        PrivatePostDocument document{bundle.metadata, payload.markdown};
        auto document_guard = MakeScopedCleanse(document);
        ValidatePrivatePostDocument(document);
        document_guard.Release();
        return document;
    } catch (const nlohmann::json::exception&) {
        throw std::runtime_error("invalid private post payload schema");
    }
}
