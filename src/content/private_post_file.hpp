#pragma once

#include <filesystem>

#include "content/private_post.hpp"

PrivatePostDocument LoadPrivatePostDocumentFile(
    const std::filesystem::path& path);

EncryptedPrivatePostBundle LoadEncryptedPrivatePostBundleFile(
    const std::filesystem::path& path);

void SaveEncryptedPrivatePostBundleFile(
    const std::filesystem::path& path,
    const EncryptedPrivatePostBundle& bundle);
