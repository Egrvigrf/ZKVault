#pragma once

#include <string>
#include <vector>

#include "model/encrypted_entry_file.hpp"
#include "model/master_key_file.hpp"
#include "model/password_entry.hpp"

std::string NowIso8601();

std::string EntryPathFor(const std::string& name);

std::vector<unsigned char> UnlockDataKey(const std::string& master_password);

MasterKeyFile CreateMasterKeyFile(
    const std::string& master_password,
    const std::vector<unsigned char>& dek);

EncryptedEntryFile EncryptPasswordEntry(
    const PasswordEntry& entry,
    const std::vector<unsigned char>& dek);

PasswordEntry DecryptPasswordEntry(
    const EncryptedEntryFile& file,
    const std::vector<unsigned char>& dek);
