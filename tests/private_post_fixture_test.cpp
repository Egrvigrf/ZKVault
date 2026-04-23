#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#include "content/private_post.hpp"
#include "content/private_post_file.hpp"

namespace {

void Require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

std::filesystem::path FixturePath(std::string_view relative_path) {
    return std::filesystem::path(ZKVAULT_SOURCE_DIR) / relative_path;
}

void TestReferenceFixtureRoundTrip() {
    const PrivatePostDocument expected_document =
        LoadPrivatePostDocumentFile(
            FixturePath("fixtures/private-post/v1/reference-hello/document.json"));
    const EncryptedPrivatePostBundle fixture_bundle =
        LoadEncryptedPrivatePostBundleFile(
            FixturePath("fixtures/private-post/v1/reference-hello/bundle.json"));

    Require(fixture_bundle.version == 1,
            "fixture bundle version should stay pinned to v1");
    Require(fixture_bundle.payload_format == "markdown",
            "fixture bundle payload format should stay pinned");
    Require(fixture_bundle.cipher == "aes-256-gcm",
            "fixture bundle cipher should stay pinned");
    Require(fixture_bundle.kdf == "scrypt",
            "fixture bundle kdf should stay pinned");
    Require(fixture_bundle.salt == "734821130d063b02f2f468fedeb2c2ae",
            "fixture bundle salt changed unexpectedly");
    Require(fixture_bundle.data_iv == "5ee6c0060ebbfb764d14fc78",
            "fixture bundle data_iv changed unexpectedly");
    Require(
        fixture_bundle.ciphertext ==
            "2c0dfa30a9a63a6996c05748ca585af6940270b0b6bc7429eec76abf89f7114a20085b5e2be4292d80a69fecbeae94a2e44a13eaf8661144d4ea2e4904d635e2a46f63eb13caa695039b6ac049d802f29a595a5abac1c800a007d90951dd710e8c86e482118d98322643b75446",
        "fixture bundle ciphertext changed unexpectedly");
    Require(fixture_bundle.auth_tag == "5d122cd764035dd3ab12d645b80a5485",
            "fixture bundle auth_tag changed unexpectedly");

    const PrivatePostDocument decrypted_document =
        DecryptPrivatePostDocument(fixture_bundle, "fixture-password-v1");

    Require(decrypted_document.metadata.slug == expected_document.metadata.slug,
            "fixture slug should decrypt to expected document");
    Require(decrypted_document.metadata.title == expected_document.metadata.title,
            "fixture title should decrypt to expected document");
    Require(
        decrypted_document.metadata.excerpt == expected_document.metadata.excerpt,
        "fixture excerpt should decrypt to expected document");
    Require(
        decrypted_document.metadata.published_at ==
            expected_document.metadata.published_at,
        "fixture published_at should decrypt to expected document");
    Require(decrypted_document.markdown == expected_document.markdown,
            "fixture markdown should decrypt to expected document");
}

}  // namespace

int main() {
    TestReferenceFixtureRoundTrip();
    return 0;
}
