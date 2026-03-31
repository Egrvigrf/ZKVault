#pragma once

#include <string>
#include <vector>

#include "model/encrypted_entry_file.hpp"

void SaveEncryptedEntryFile(const std::string& name, const EncryptedEntryFile& file);
EncryptedEntryFile LoadEncryptedEntryFile(const std::string& name);
bool PasswordEntryExists(const std::string& name);
void DeletePasswordEntry(const std::string& name);
std::vector<std::string> ListPasswordEntries();
