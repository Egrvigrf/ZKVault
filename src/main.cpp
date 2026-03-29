#include <iostream>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

#include <openssl/crypto.h>

#include "crypto/aead.hpp"
#include "crypto/hex.hpp"
#include "crypto/kdf.hpp"
#include "crypto/random.hpp"
#include "model/encrypted_entry_file.hpp"
#include "model/master_key_file.hpp"
#include "model/password_entry.hpp"
#include "storage/master_key_storage.hpp"
#include "storage/json_storage.hpp"

namespace {

void CleanseString(std::string& value) {
    if (!value.empty()) {
        OPENSSL_cleanse(value.data(), value.size());
    }
}

void CleanseBytes(std::vector<unsigned char>& value) {
    if (!value.empty()) {
        OPENSSL_cleanse(value.data(), value.size());
    }
}

std::string ReadSecret(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    if (!isatty(STDIN_FILENO)) {
        std::string secret;
        std::getline(std::cin, secret);
        return secret;
    }

    termios old_settings{};
    if (tcgetattr(STDIN_FILENO, &old_settings) != 0) {
        throw std::runtime_error("failed to read terminal settings");
    }

    termios new_settings = old_settings;
    new_settings.c_lflag &= ~ECHO;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_settings) != 0) {
        throw std::runtime_error("failed to disable terminal echo");
    }

    std::string secret;
    std::getline(std::cin, secret);
    std::cout << '\n';

    if (tcsetattr(STDIN_FILENO, TCSANOW, &old_settings) != 0) {
        throw std::runtime_error("failed to restore terminal settings");
    }

    return secret;
}

std::string ReadLine(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    std::string value;
    std::getline(std::cin, value);
    return value;
}

std::vector<unsigned char> UnlockDataKey(const std::string& master_password) {
    const MasterKeyFile master_key_file = LoadMasterKeyFile();

    std::vector<unsigned char> salt = HexToBytes(master_key_file.salt);
    std::vector<unsigned char> wrap_iv = HexToBytes(master_key_file.wrap_iv);
    std::vector<unsigned char> encrypted_dek =
        HexToBytes(master_key_file.encrypted_dek);
    std::vector<unsigned char> auth_tag =
        HexToBytes(master_key_file.auth_tag);

    std::vector<unsigned char> kek =
        DeriveKeyScrypt(master_password, salt, 32);
    std::vector<unsigned char> dek =
        DecryptAes256Gcm(kek, wrap_iv, encrypted_dek, auth_tag);

    CleanseBytes(kek);
    CleanseBytes(salt);
    CleanseBytes(wrap_iv);
    CleanseBytes(encrypted_dek);
    CleanseBytes(auth_tag);

    return dek;
}

EncryptedEntryFile EncryptPasswordEntry(
    const PasswordEntry& entry,
    const std::vector<unsigned char>& dek) {
    const json serialized = entry;
    std::string plaintext = serialized.dump();
    std::vector<unsigned char> plaintext_bytes(plaintext.begin(), plaintext.end());
    const AeadCiphertext encrypted = EncryptAes256Gcm(dek, plaintext_bytes);
    CleanseString(plaintext);
    CleanseBytes(plaintext_bytes);

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
    std::vector<unsigned char> ciphertext = HexToBytes(file.ciphertext);
    std::vector<unsigned char> auth_tag = HexToBytes(file.auth_tag);
    std::vector<unsigned char> plaintext_bytes =
        DecryptAes256Gcm(dek, iv, ciphertext, auth_tag);
    std::string plaintext(plaintext_bytes.begin(), plaintext_bytes.end());
    const json serialized = json::parse(plaintext);
    const PasswordEntry entry = serialized.get<PasswordEntry>();
    CleanseBytes(iv);
    CleanseBytes(ciphertext);
    CleanseBytes(auth_tag);
    CleanseBytes(plaintext_bytes);
    CleanseString(plaintext);
    return entry;
}

void PrintUsage() {
    std::cout << "Usage:\n";
    std::cout << "  zkvault init\n";
    std::cout << "  zkvault add <name>\n";
    std::cout << "  zkvault get <name>\n";
    std::cout << "  zkvault update <name>\n";
    std::cout << "  zkvault delete <name>\n";
    std::cout << "  zkvault list\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            PrintUsage();
            return 1;
        }

        std::string command = argv[1];

        if (command == "init") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            std::string master_password = ReadSecret("Master password: ");
            std::vector<unsigned char> salt = GenerateRandomBytes(16);
            std::vector<unsigned char> kek = DeriveKeyScrypt(master_password, salt, 32);
            std::vector<unsigned char> dek = GenerateRandomBytes(32);
            const AeadCiphertext wrapped_dek = EncryptAes256Gcm(kek, dek);

            MasterKeyFile master_key_file{
                1,
                "scrypt",
                BytesToHex(salt),
                BytesToHex(wrapped_dek.iv),
                BytesToHex(wrapped_dek.ciphertext),
                BytesToHex(wrapped_dek.auth_tag)
            };

            SaveMasterKeyFile(master_key_file);
            CleanseString(master_password);
            CleanseBytes(kek);
            CleanseBytes(dek);
            CleanseBytes(salt);
            std::cout << "initialized .zkv_master\n";
            return 0;
        }

        if (command == "add" || command == "update") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            std::string master_password = ReadSecret("Master password: ");
            std::vector<unsigned char> dek = UnlockDataKey(master_password);
            PasswordEntry entry{
                argv[2],
                ReadSecret("Entry password: "),
                ReadLine("Note: ")
            };

            const EncryptedEntryFile encrypted = EncryptPasswordEntry(entry, dek);
            SaveEncryptedEntryFile(entry.name, encrypted);
            CleanseString(master_password);
            CleanseString(entry.password);
            CleanseString(entry.note);
            CleanseBytes(dek);
            std::cout << (command == "add" ? "saved to " : "updated ")
                      << "data/" << entry.name << ".zkv\n";
            return 0;
        }

        if (command == "get") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            std::string master_password = ReadSecret("Master password: ");
            std::vector<unsigned char> dek = UnlockDataKey(master_password);
            const EncryptedEntryFile encrypted = LoadEncryptedEntryFile(argv[2]);
            PasswordEntry entry = DecryptPasswordEntry(encrypted, dek);
            json serialized = entry;
            std::string output = serialized.dump(2);
            CleanseString(master_password);
            CleanseBytes(dek);
            std::cout << output << '\n';
            CleanseString(entry.password);
            CleanseString(entry.note);
            CleanseString(output);
            return 0;
        }

        if (command == "delete") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            DeletePasswordEntry(argv[2]);
            std::cout << "deleted data/" << argv[2] << ".zkv\n";
            return 0;
        }

        if (command == "list") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            for (const std::string& name : ListPasswordEntries()) {
                std::cout << name << '\n';
            }
            return 0;
        }

        PrintUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
