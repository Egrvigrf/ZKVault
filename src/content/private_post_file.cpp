#include "content/private_post_file.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "storage/atomic_file.hpp"

namespace {

template <typename T>
T ReadJsonFile(
    const std::filesystem::path& path,
    const char* open_error,
    const char* json_error,
    const char* schema_error) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(open_error);
    }

    nlohmann::json serialized;
    try {
        input >> serialized;
    } catch (const nlohmann::json::exception&) {
        throw std::runtime_error(json_error);
    }

    try {
        return serialized.get<T>();
    } catch (const nlohmann::json::exception&) {
        throw std::runtime_error(schema_error);
    }
}

}  // namespace

PrivatePostDocument LoadPrivatePostDocumentFile(
    const std::filesystem::path& path) {
    PrivatePostDocument document = ReadJsonFile<PrivatePostDocument>(
        path,
        "failed to open private post document file",
        "invalid private post document JSON",
        "invalid private post document schema");
    ValidatePrivatePostDocument(document);
    return document;
}

EncryptedPrivatePostBundle LoadEncryptedPrivatePostBundleFile(
    const std::filesystem::path& path) {
    EncryptedPrivatePostBundle bundle = ReadJsonFile<EncryptedPrivatePostBundle>(
        path,
        "failed to open encrypted private post bundle file",
        "invalid encrypted private post bundle JSON",
        "invalid encrypted private post bundle schema");
    ValidateEncryptedPrivatePostBundle(bundle);
    return bundle;
}

void SaveEncryptedPrivatePostBundleFile(
    const std::filesystem::path& path,
    const EncryptedPrivatePostBundle& bundle) {
    ValidateEncryptedPrivatePostBundle(bundle);
    const std::string contents = nlohmann::json(bundle).dump(2);
    WriteFileAtomically(path, contents);
}
