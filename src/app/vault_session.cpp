#include "app/vault_session.hpp"

#include <utility>

#include "app/vault_codec.hpp"
#include "crypto/secure_memory.hpp"
#include "storage/json_storage.hpp"
#include "storage/master_key_storage.hpp"

VaultSession VaultSession::Open(const std::string& master_password) {
    return VaultSession(UnlockDataKey(master_password));
}

VaultSession::VaultSession(std::vector<unsigned char> data_key)
    : data_key_(std::move(data_key)) {}

VaultSession::VaultSession(VaultSession&& other) noexcept
    : data_key_(std::move(other.data_key_)) {}

VaultSession& VaultSession::operator=(VaultSession&& other) noexcept {
    if (this != &other) {
        Cleanse(data_key_);
        data_key_ = std::move(other.data_key_);
    }

    return *this;
}

VaultSession::~VaultSession() {
    Cleanse(data_key_);
}

StorePasswordEntryResult VaultSession::StoreEntry(
    const StorePasswordEntryRequest& request) {
    const bool entry_exists = PasswordEntryExists(request.name);
    if (request.mode == EntryMutationMode::kCreate && entry_exists) {
        throw std::runtime_error("entry already exists");
    }

    if (request.mode == EntryMutationMode::kUpdate && !entry_exists) {
        throw std::runtime_error("entry does not exist");
    }

    const std::string now = NowIso8601();
    std::string created_at = now;

    if (request.mode == EntryMutationMode::kUpdate) {
        PasswordEntry old_entry =
            DecryptPasswordEntry(LoadEncryptedEntryFile(request.name), data_key_);
        auto old_entry_guard = MakeScopedCleanse(old_entry);
        if (!old_entry.created_at.empty()) {
            created_at = old_entry.created_at;
        }
    }

    PasswordEntry entry{
        request.name,
        request.password,
        request.note,
        created_at,
        now
    };
    auto entry_guard = MakeScopedCleanse(entry);

    const EncryptedEntryFile encrypted = EncryptPasswordEntry(entry, data_key_);
    SaveEncryptedEntryFile(entry.name, encrypted);

    return StorePasswordEntryResult{
        EntryPathFor(entry.name),
        entry.created_at,
        entry.updated_at
    };
}

PasswordEntry VaultSession::LoadEntry(const std::string& name) const {
    if (!PasswordEntryExists(name)) {
        throw std::runtime_error("entry does not exist");
    }

    const EncryptedEntryFile encrypted = LoadEncryptedEntryFile(name);
    return DecryptPasswordEntry(encrypted, data_key_);
}

RemovePasswordEntryResult VaultSession::RemoveEntry(const std::string& name) const {
    if (!PasswordEntryExists(name)) {
        throw std::runtime_error("entry does not exist");
    }

    DeletePasswordEntry(name);
    return RemovePasswordEntryResult{EntryPathFor(name)};
}

RotateMasterPasswordResult VaultSession::RotateMasterPassword(
    const std::string& new_master_password) const {
    const MasterKeyFile master_key_file =
        CreateMasterKeyFile(new_master_password, data_key_);

    OverwriteMasterKeyFile(master_key_file);
    return RotateMasterPasswordResult{".zkv_master"};
}

std::vector<std::string> VaultSession::ListEntryNames() const {
    return ListPasswordEntries();
}

bool VaultSession::EntryExists(const std::string& name) const {
    return PasswordEntryExists(name);
}
