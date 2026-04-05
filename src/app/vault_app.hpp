#pragma once

#include <string>
#include <vector>

#include "crypto/secure_memory.hpp"
#include "model/password_entry.hpp"

enum class EntryMutationMode {
    kCreate,
    kUpdate
};

struct InitializeVaultRequest {
    std::string master_password;
};

struct InitializeVaultResult {
    std::string master_key_path;
};

struct RotateMasterPasswordRequest {
    std::string current_master_password;
    std::string new_master_password;
};

struct RotateMasterPasswordResult {
    std::string master_key_path;
};

struct StorePasswordEntryRequest {
    EntryMutationMode mode;
    std::string name;
    std::string master_password;
    std::string password;
    std::string note;
};

struct StorePasswordEntryResult {
    std::string entry_path;
    std::string created_at;
    std::string updated_at;
};

struct LoadPasswordEntryRequest {
    std::string name;
    std::string master_password;
};

struct RemovePasswordEntryRequest {
    std::string name;
};

struct RemovePasswordEntryResult {
    std::string entry_path;
};

inline void Cleanse(InitializeVaultRequest& request) {
    Cleanse(request.master_password);
}

inline void Cleanse(RotateMasterPasswordRequest& request) {
    Cleanse(request.current_master_password);
    Cleanse(request.new_master_password);
}

inline void Cleanse(StorePasswordEntryRequest& request) {
    Cleanse(request.master_password);
    Cleanse(request.password);
    Cleanse(request.note);
}

inline void Cleanse(LoadPasswordEntryRequest& request) {
    Cleanse(request.master_password);
}

InitializeVaultResult InitializeVault(const InitializeVaultRequest& request);

RotateMasterPasswordResult RotateMasterPassword(
    const RotateMasterPasswordRequest& request);

StorePasswordEntryResult StorePasswordEntry(
    const StorePasswordEntryRequest& request);

PasswordEntry LoadPasswordEntry(const LoadPasswordEntryRequest& request);

RemovePasswordEntryResult RemovePasswordEntry(
    const RemovePasswordEntryRequest& request);

std::vector<std::string> ListEntryNames();

bool EntryExists(const std::string& name);
