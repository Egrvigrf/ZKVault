#include <iostream>
#include <stdexcept>
#include <string>

#include "app/vault_app.hpp"
#include "crypto/secure_memory.hpp"
#include "model/password_entry.hpp"
#include "shell/interactive_shell.hpp"
#include "terminal/prompt.hpp"

namespace {

void PrintUsage() {
    std::cout << "Usage:\n";
    std::cout << "  zkvault init\n";
    std::cout << "  zkvault shell\n";
    std::cout << "  zkvault change-master-password\n";
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

        const std::string command = argv[1];

        if (command == "shell") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            return RunInteractiveShell();
        }

        if (command == "init") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            InitializeVaultRequest request{
                ReadConfirmedSecret(
                    "Master password: ",
                    "Confirm master password: ",
                    "master passwords do not match")
            };
            auto request_guard = MakeScopedCleanse(request);
            const InitializeVaultResult result = InitializeVault(request);
            std::cout << "initialized " << result.master_key_path << '\n';
            return 0;
        }

        if (command == "change-master-password") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            RequireExactConfirmation(
                "Type CHANGE to confirm master password rotation: ",
                "CHANGE",
                "master password rotation cancelled");
            RotateMasterPasswordRequest request{
                ReadSecret("Current master password: "),
                ReadConfirmedSecret(
                    "New master password: ",
                    "Confirm new master password: ",
                    "new master passwords do not match")
            };
            auto request_guard = MakeScopedCleanse(request);
            const RotateMasterPasswordResult result = RotateMasterPassword(request);
            std::cout << "updated " << result.master_key_path << '\n';
            return 0;
        }

        if (command == "add" || command == "update") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            const bool entry_exists = EntryExists(argv[2]);
            if (command == "add" && entry_exists) {
                throw std::runtime_error("entry already exists");
            }

            if (command == "update" && !entry_exists) {
                throw std::runtime_error("entry does not exist");
            }

            if (command == "update") {
                RequireExactConfirmation(
                    "Type the entry name to confirm overwrite: ",
                    argv[2],
                    "entry overwrite cancelled");
            }

            StorePasswordEntryRequest request{
                command == "add" ? EntryMutationMode::kCreate
                                 : EntryMutationMode::kUpdate,
                argv[2],
                ReadSecret("Master password: "),
                ReadSecret("Entry password: "),
                ReadLine("Note: ")
            };
            auto request_guard = MakeScopedCleanse(request);
            const StorePasswordEntryResult result = StorePasswordEntry(request);
            std::cout << (command == "add" ? "saved to " : "updated ")
                      << result.entry_path << '\n';
            return 0;
        }

        if (command == "get") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            if (!EntryExists(argv[2])) {
                throw std::runtime_error("entry does not exist");
            }

            LoadPasswordEntryRequest request{
                argv[2],
                ReadSecret("Master password: ")
            };
            auto request_guard = MakeScopedCleanse(request);
            PasswordEntry entry = LoadPasswordEntry(request);
            auto entry_guard = MakeScopedCleanse(entry);
            std::string output = json(entry).dump(2);
            auto output_guard = MakeScopedCleanse(output);
            std::cout << output << '\n';
            return 0;
        }

        if (command == "delete") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            if (!EntryExists(argv[2])) {
                throw std::runtime_error("entry does not exist");
            }

            RequireExactConfirmation(
                "Type the entry name to confirm deletion: ",
                argv[2],
                "entry deletion cancelled");
            const RemovePasswordEntryResult result =
                RemovePasswordEntry(RemovePasswordEntryRequest{argv[2]});
            std::cout << "deleted " << result.entry_path << '\n';
            return 0;
        }

        if (command == "list") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            for (const std::string& name : ListEntryNames()) {
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
