#pragma once

#include <string>
#include <vector>

#include "app/vault_app.hpp"

class VaultSession {
public:
    static VaultSession Open(const std::string& master_password);

    VaultSession(const VaultSession&) = delete;
    VaultSession& operator=(const VaultSession&) = delete;
    VaultSession(VaultSession&& other) noexcept;
    VaultSession& operator=(VaultSession&& other) noexcept;
    ~VaultSession();

    StorePasswordEntryResult StoreEntry(const StorePasswordEntryRequest& request);

    PasswordEntry LoadEntry(const std::string& name) const;

    RemovePasswordEntryResult RemoveEntry(const std::string& name) const;

    RotateMasterPasswordResult RotateMasterPassword(
        const std::string& new_master_password) const;

    std::vector<std::string> ListEntryNames() const;

    bool EntryExists(const std::string& name) const;

private:
    explicit VaultSession(std::vector<unsigned char> data_key);

    std::vector<unsigned char> data_key_;
};
