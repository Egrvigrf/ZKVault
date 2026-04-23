#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "content/private_post.hpp"

namespace {

void Require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void RequireThrows(
    const std::function<void()>& fn,
    std::string_view expected_message) {
    try {
        fn();
    } catch (const std::exception& ex) {
        Require(ex.what() == expected_message,
                "exception message should match expected private post contract");
        return;
    }

    throw std::runtime_error("expected function to throw");
}

void TestSlugValidation() {
    ValidatePrivatePostSlug("notes/private-post");
    ValidatePrivatePostSlug("2026/04/halo-update");

    RequireThrows(
        [] { ValidatePrivatePostSlug(""); },
        "private post slug must not be empty");
    RequireThrows(
        [] { ValidatePrivatePostSlug("/leading"); },
        "private post slug must not start or end with '/'");
    RequireThrows(
        [] { ValidatePrivatePostSlug("trailing/"); },
        "private post slug must not start or end with '/'");
    RequireThrows(
        [] { ValidatePrivatePostSlug("double//slash"); },
        "private post slug must not contain empty path segments");
    RequireThrows(
        [] { ValidatePrivatePostSlug("bad space"); },
        "private post slug may only contain letters, digits, '.', '-', '_' and '/'");
}

void TestDocumentValidation() {
    const PrivatePostDocument document{
        PrivatePostMetadata{
            "notes/private-post",
            "Private Post",
            "Preview text",
            "2026-04-23T00:00:00Z"
        },
        "# Locked\nsecret body"
    };

    ValidatePrivatePostDocument(document);

    RequireThrows(
        [] {
            ValidatePrivatePostDocument(
                PrivatePostDocument{
                    PrivatePostMetadata{
                        "notes/private-post",
                        "",
                        "",
                        ""
                    },
                    "body"
                });
        },
        "private post title must not be empty");
}

void TestBundleRoundTrip() {
    const std::string access_password = "open-sesame";
    const PrivatePostDocument original{
        PrivatePostMetadata{
            "notes/private-post",
            "Private Post",
            "Preview text",
            "2026-04-23T00:00:00Z"
        },
        "# Locked\nsecret body"
    };

    const EncryptedPrivatePostBundle encrypted =
        EncryptPrivatePostDocument(original, access_password);
    ValidateEncryptedPrivatePostBundle(encrypted);
    Require(encrypted.version == 1, "private post bundle version should be 1");
    Require(encrypted.payload_format == "markdown",
            "private post bundle should record markdown payloads");
    Require(encrypted.cipher == "aes-256-gcm",
            "private post bundle should record AES-GCM");
    Require(encrypted.kdf == "scrypt",
            "private post bundle should record scrypt");
    Require(encrypted.metadata.slug == original.metadata.slug,
            "private post bundle should preserve public metadata");
    Require(!encrypted.ciphertext.empty(),
            "private post bundle should contain ciphertext");

    const PrivatePostDocument decrypted =
        DecryptPrivatePostDocument(encrypted, access_password);
    Require(decrypted.metadata.slug == original.metadata.slug,
            "decrypted private post slug should match");
    Require(decrypted.metadata.title == original.metadata.title,
            "decrypted private post title should match");
    Require(decrypted.markdown == original.markdown,
            "decrypted private post markdown should match");
}

void TestBundleJsonRoundTrip() {
    const EncryptedPrivatePostBundle encrypted = EncryptPrivatePostDocument(
        PrivatePostDocument{
            PrivatePostMetadata{
                "notes/private-post",
                "Private Post",
                "Preview text",
                "2026-04-23T00:00:00Z"
            },
            "# Locked\nsecret body"
        },
        "open-sesame");

    const nlohmann::json serialized = encrypted;
    const EncryptedPrivatePostBundle reparsed =
        serialized.get<EncryptedPrivatePostBundle>();
    ValidateEncryptedPrivatePostBundle(reparsed);
    Require(reparsed.metadata.title == encrypted.metadata.title,
            "reparsed private post title should match");
    Require(reparsed.ciphertext == encrypted.ciphertext,
            "reparsed private post ciphertext should match");
}

void TestWrongPasswordFails() {
    const EncryptedPrivatePostBundle encrypted = EncryptPrivatePostDocument(
        PrivatePostDocument{
            PrivatePostMetadata{
                "notes/private-post",
                "Private Post",
                "Preview text",
                "2026-04-23T00:00:00Z"
            },
            "# Locked\nsecret body"
        },
        "open-sesame");

    RequireThrows(
        [&] { static_cast<void>(DecryptPrivatePostDocument(encrypted, "wrong")); },
        "AES-256-GCM decryption failed");
    RequireThrows(
        [&] { static_cast<void>(EncryptPrivatePostDocument(
                    PrivatePostDocument{
                        PrivatePostMetadata{
                            "notes/private-post",
                            "Private Post",
                            "",
                            ""
                        },
                        "body"
                    },
                    "")); },
        "private post access password must not be empty");
}

}  // namespace

int main() {
    TestSlugValidation();
    TestDocumentValidation();
    TestBundleRoundTrip();
    TestBundleJsonRoundTrip();
    TestWrongPasswordFails();
    return 0;
}
