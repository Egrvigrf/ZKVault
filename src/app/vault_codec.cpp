#include "app/vault_codec.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "crypto/aead.hpp"
#include "crypto/hex.hpp"
#include "crypto/kdf.hpp"
#include "crypto/random.hpp"
#include "crypto/secure_memory.hpp"
#include "storage/master_key_storage.hpp"

std::string NowIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
    gmtime_r(&t, &tm_utc);
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string EntryPathFor(const std::string& name) {
    return "data/" + name + ".zkv";
}

std::vector<unsigned char> UnlockDataKey(const std::string& master_password) {
    const MasterKeyFile master_key_file = LoadMasterKeyFile();

    std::vector<unsigned char> salt = HexToBytes(master_key_file.salt);
    auto salt_guard = MakeScopedCleanse(salt);
    std::vector<unsigned char> wrap_iv = HexToBytes(master_key_file.wrap_iv);
    auto wrap_iv_guard = MakeScopedCleanse(wrap_iv);
    std::vector<unsigned char> encrypted_dek =
        HexToBytes(master_key_file.encrypted_dek);
    auto encrypted_dek_guard = MakeScopedCleanse(encrypted_dek);
    std::vector<unsigned char> auth_tag =
        HexToBytes(master_key_file.auth_tag);
    auto auth_tag_guard = MakeScopedCleanse(auth_tag);

    std::vector<unsigned char> kek =
        DeriveKeyScrypt(master_password, salt, 32);
    auto kek_guard = MakeScopedCleanse(kek);
    std::vector<unsigned char> dek =
        DecryptAes256Gcm(kek, wrap_iv, encrypted_dek, auth_tag);
    auto dek_guard = MakeScopedCleanse(dek);

    dek_guard.Release();
    return dek;
}

MasterKeyFile CreateMasterKeyFile(
    const std::string& master_password,
    const std::vector<unsigned char>& dek) {
    std::vector<unsigned char> salt = GenerateRandomBytes(16);
    auto salt_guard = MakeScopedCleanse(salt);
    std::vector<unsigned char> kek = DeriveKeyScrypt(master_password, salt, 32);
    auto kek_guard = MakeScopedCleanse(kek);
    const AeadCiphertext wrapped_dek = EncryptAes256Gcm(kek, dek);

    return MasterKeyFile{
        1,
        "scrypt",
        BytesToHex(salt),
        BytesToHex(wrapped_dek.iv),
        BytesToHex(wrapped_dek.ciphertext),
        BytesToHex(wrapped_dek.auth_tag)
    };
}

EncryptedEntryFile EncryptPasswordEntry(
    const PasswordEntry& entry,
    const std::vector<unsigned char>& dek) {
    std::string plaintext = json(entry).dump();
    auto plaintext_guard = MakeScopedCleanse(plaintext);
    std::vector<unsigned char> plaintext_bytes(plaintext.begin(), plaintext.end());
    auto plaintext_bytes_guard = MakeScopedCleanse(plaintext_bytes);
    const AeadCiphertext encrypted = EncryptAes256Gcm(dek, plaintext_bytes);

    return EncryptedEntryFile{
        1,
        BytesToHex(encrypted.iv),
        BytesToHex(encrypted.ciphertext),
        BytesToHex(encrypted.auth_tag)
    };
}

PasswordEntry DecryptPasswordEntry(
    const EncryptedEntryFile& file,
    const std::vector<unsigned char>& dek) {
    std::vector<unsigned char> iv = HexToBytes(file.data_iv);
    auto iv_guard = MakeScopedCleanse(iv);
    std::vector<unsigned char> ciphertext = HexToBytes(file.ciphertext);
    auto ciphertext_guard = MakeScopedCleanse(ciphertext);
    std::vector<unsigned char> auth_tag = HexToBytes(file.auth_tag);
    auto auth_tag_guard = MakeScopedCleanse(auth_tag);
    std::vector<unsigned char> plaintext_bytes =
        DecryptAes256Gcm(dek, iv, ciphertext, auth_tag);
    auto plaintext_bytes_guard = MakeScopedCleanse(plaintext_bytes);
    std::string plaintext(plaintext_bytes.begin(), plaintext_bytes.end());
    auto plaintext_guard = MakeScopedCleanse(plaintext);
    PasswordEntry entry = json::parse(plaintext).get<PasswordEntry>();
    auto entry_guard = MakeScopedCleanse(entry);
    entry_guard.Release();
    return entry;
}
