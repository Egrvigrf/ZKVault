#include "storage/json_storage.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <stdexcept>

namespace {

std::filesystem::path EntryPath(const std::string& name) {
    return std::filesystem::path("data") / (name + ".zkv");
}

}  // namespace

void SaveEncryptedEntryFile(const std::string& name, const EncryptedEntryFile& file) {
    std::filesystem::create_directories("data");

    std::ofstream output(EntryPath(name));
    if (!output) {
        throw std::runtime_error("failed to open output file");
    }

    json serialized = file;
    output << serialized.dump(2);
}

EncryptedEntryFile LoadEncryptedEntryFile(const std::string& name) {
    std::ifstream input(EntryPath(name));
    if (!input) {
        throw std::runtime_error("failed to open input file");
    }

    json serialized;
    input >> serialized;

    return serialized.get<EncryptedEntryFile>();
}

void DeletePasswordEntry(const std::string& name) {
    const std::filesystem::path path = EntryPath(name);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("entry does not exist");
    }

    if (!std::filesystem::remove(path)) {
        throw std::runtime_error("failed to delete entry");
    }
}

std::vector<std::string> ListPasswordEntries() {
    std::vector<std::string> names;
    const std::filesystem::path data_dir = "data";

    if (!std::filesystem::exists(data_dir)) {
        return names;
    }

    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::filesystem::path path = entry.path();
        if (path.extension() == ".zkv") {
            names.push_back(path.stem().string());
        }
    }

    std::sort(names.begin(), names.end());
    return names;
}
