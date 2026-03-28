#include "storage/master_key_storage.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

const std::filesystem::path kMasterKeyPath = ".zkv_master";

}  // namespace

void SaveMasterKeyFile(const MasterKeyFile& file) {
    if (std::filesystem::exists(kMasterKeyPath)) {
        throw std::runtime_error(".zkv_master already exists");
    }

    std::ofstream output(kMasterKeyPath);
    if (!output) {
        throw std::runtime_error("failed to open .zkv_master");
    }

    json serialized = file;
    output << serialized.dump(2);
}

MasterKeyFile LoadMasterKeyFile() {
    std::ifstream input(kMasterKeyPath);
    if (!input) {
        throw std::runtime_error("failed to open .zkv_master");
    }

    json serialized;
    input >> serialized;

    return serialized.get<MasterKeyFile>();
}
