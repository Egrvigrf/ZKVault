#include "app/vault_app.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include "app/vault_codec.hpp"
#include "app/vault_session.hpp"
#include "crypto/random.hpp"
#include "crypto/secure_memory.hpp"
#include "storage/json_storage.hpp"
#include "storage/master_key_storage.hpp"

InitializeVaultResult InitializeVault(const InitializeVaultRequest& request) {
    std::vector<unsigned char> dek = GenerateRandomBytes(32);
    auto dek_guard = MakeScopedCleanse(dek);
    const MasterKeyFile master_key_file =
        CreateMasterKeyFile(request.master_password, dek);

    SaveMasterKeyFile(master_key_file);
    return InitializeVaultResult{".zkv_master"};
}

RotateMasterPasswordResult RotateMasterPassword(
    const RotateMasterPasswordRequest& request) {
    VaultSession session = VaultSession::Open(request.current_master_password);
    return session.RotateMasterPassword(request.new_master_password);
}

StorePasswordEntryResult StorePasswordEntry(
    const StorePasswordEntryRequest& request) {
    VaultSession session = VaultSession::Open(request.master_password);
    return session.StoreEntry(request);
}

PasswordEntry LoadPasswordEntry(const LoadPasswordEntryRequest& request) {
    VaultSession session = VaultSession::Open(request.master_password);
    return session.LoadEntry(request.name);
}

RemovePasswordEntryResult RemovePasswordEntry(
    const RemovePasswordEntryRequest& request) {
    if (!PasswordEntryExists(request.name)) {
        throw std::runtime_error("entry does not exist");
    }

    ::DeletePasswordEntry(request.name);
    return RemovePasswordEntryResult{EntryPathFor(request.name)};
}

std::vector<std::string> ListEntryNames() {
    return ListPasswordEntries();
}

bool EntryExists(const std::string& name) {
    return PasswordEntryExists(name);
}
