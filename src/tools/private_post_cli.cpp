#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "content/private_post.hpp"
#include "content/private_post_file.hpp"
#include "crypto/secure_memory.hpp"
#include "terminal/prompt.hpp"

namespace {

int PrintUsageAndReturn() {
    std::cerr
        << "usage:\n"
        << "  zkvault encrypt-post <document-path> <bundle-path>\n"
        << "  zkvault decrypt-post-preview <bundle-path>\n"
        << "  zkvault validate-document <document-path>\n"
        << "  zkvault validate-bundle <bundle-path>\n";
    return 1;
}

int RunEncryptPostCommand(int argc, char* argv[]) {
    if (argc != 4) {
        return PrintUsageAndReturn();
    }

    const std::filesystem::path document_path = argv[2];
    const std::filesystem::path bundle_path = argv[3];
    PrivatePostDocument document = LoadPrivatePostDocumentFile(document_path);
    auto document_guard = MakeScopedCleanse(document);
    std::string access_password = ReadConfirmedSecret(
        "Access password: ",
        "Confirm access password: ",
        "access passwords do not match");
    auto access_password_guard = MakeScopedCleanse(access_password);

    EncryptedPrivatePostBundle bundle =
        EncryptPrivatePostDocument(document, access_password);
    auto bundle_guard = MakeScopedCleanse(bundle);
    SaveEncryptedPrivatePostBundleFile(bundle_path, bundle);
    std::cout << "encrypted private post saved to " << bundle_path.string()
              << '\n';
    return 0;
}

int RunDecryptPostPreviewCommand(int argc, char* argv[]) {
    if (argc != 3) {
        return PrintUsageAndReturn();
    }

    const std::filesystem::path bundle_path = argv[2];
    EncryptedPrivatePostBundle bundle =
        LoadEncryptedPrivatePostBundleFile(bundle_path);
    auto bundle_guard = MakeScopedCleanse(bundle);
    std::string access_password = ReadSecret("Access password: ");
    auto access_password_guard = MakeScopedCleanse(access_password);

    PrivatePostDocument document =
        DecryptPrivatePostDocument(bundle, access_password);
    auto document_guard = MakeScopedCleanse(document);
    std::string output = nlohmann::json(document).dump(2);
    auto output_guard = MakeScopedCleanse(output);
    std::cout << output << '\n';
    return 0;
}

int RunValidateDocumentCommand(int argc, char* argv[]) {
    if (argc != 3) {
        return PrintUsageAndReturn();
    }

    const std::filesystem::path document_path = argv[2];
    LoadPrivatePostDocumentFile(document_path);
    std::cout << "private post document is valid: " << document_path.string()
              << '\n';
    return 0;
}

int RunValidateBundleCommand(int argc, char* argv[]) {
    if (argc != 3) {
        return PrintUsageAndReturn();
    }

    const std::filesystem::path bundle_path = argv[2];
    LoadEncryptedPrivatePostBundleFile(bundle_path);
    std::cout << "encrypted private post bundle is valid: "
              << bundle_path.string() << '\n';
    return 0;
}

int DispatchCommand(int argc, char* argv[]) {
    if (argc < 2) {
        return PrintUsageAndReturn();
    }

    const std::string_view command = argv[1];
    if (command == "encrypt-post") {
        return RunEncryptPostCommand(argc, argv);
    }

    if (command == "decrypt-post-preview") {
        return RunDecryptPostPreviewCommand(argc, argv);
    }

    if (command == "validate-document") {
        return RunValidateDocumentCommand(argc, argv);
    }

    if (command == "validate-bundle") {
        return RunValidateBundleCommand(argc, argv);
    }

    return PrintUsageAndReturn();
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        return DispatchCommand(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
