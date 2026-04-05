#include "shell/interactive_shell.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/vault_app.hpp"
#include "app/vault_session.hpp"
#include "crypto/secure_memory.hpp"
#include "model/password_entry.hpp"
#include "terminal/prompt.hpp"

namespace {

std::vector<std::string> SplitCommand(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> parts;
    std::string part;
    while (input >> part) {
        parts.push_back(part);
    }
    return parts;
}

void PrintEntry(const PasswordEntry& entry) {
    std::string output = json(entry).dump(2);
    auto output_guard = MakeScopedCleanse(output);
    std::cout << output << '\n';
}

void PrintShellHelp() {
    std::cout << "Commands:\n";
    std::cout << "  help\n";
    std::cout << "  list\n";
    std::cout << "  show <name>\n";
    std::cout << "  add <name>\n";
    std::cout << "  update <name>\n";
    std::cout << "  delete <name>\n";
    std::cout << "  change-master-password\n";
    std::cout << "  quit\n";
}

VaultSession OpenOrInitializeSession() {
    if (!std::filesystem::exists(".zkv_master")) {
        const std::string choice = ReadLine(
            "Vault not initialized. Create one now? [y/N]: ");
        if (choice != "y" && choice != "Y" && choice != "yes" && choice != "YES") {
            throw std::runtime_error("vault not initialized");
        }

        InitializeVaultRequest init_request{
            ReadConfirmedSecret(
                "Master password: ",
                "Confirm master password: ",
                "master passwords do not match")
        };
        auto init_request_guard = MakeScopedCleanse(init_request);
        const InitializeVaultResult result = InitializeVault(init_request);
        std::cout << "initialized " << result.master_key_path << '\n';
        return VaultSession::Open(init_request.master_password);
    }

    std::string master_password = ReadSecret("Master password: ");
    auto master_password_guard = MakeScopedCleanse(master_password);
    return VaultSession::Open(master_password);
}

void PrintList(VaultSession& session) {
    const std::vector<std::string> names = session.ListEntryNames();
    if (names.empty()) {
        std::cout << "(empty)\n";
        return;
    }

    for (const std::string& name : names) {
        std::cout << name << '\n';
    }
}

void RequireArgumentCount(
    const std::vector<std::string>& parts,
    std::size_t expected,
    const std::string& usage) {
    if (parts.size() != expected) {
        throw std::runtime_error("usage: " + usage);
    }
}

void ExecuteShellCommand(
    VaultSession& session,
    const std::vector<std::string>& parts,
    bool& should_quit) {
    const std::string& command = parts[0];

    if (command == "help") {
        PrintShellHelp();
        return;
    }

    if (command == "list") {
        RequireArgumentCount(parts, 1, "list");
        PrintList(session);
        return;
    }

    if (command == "show") {
        RequireArgumentCount(parts, 2, "show <name>");
        PasswordEntry entry = session.LoadEntry(parts[1]);
        auto entry_guard = MakeScopedCleanse(entry);
        PrintEntry(entry);
        return;
    }

    if (command == "add") {
        RequireArgumentCount(parts, 2, "add <name>");
        StorePasswordEntryRequest request{
            EntryMutationMode::kCreate,
            parts[1],
            "",
            ReadSecret("Entry password: "),
            ReadLine("Note: ")
        };
        auto request_guard = MakeScopedCleanse(request);
        const StorePasswordEntryResult result = session.StoreEntry(request);
        std::cout << "saved to " << result.entry_path << '\n';
        return;
    }

    if (command == "update") {
        RequireArgumentCount(parts, 2, "update <name>");
        RequireExactConfirmation(
            "Type the entry name to confirm overwrite: ",
            parts[1],
            "entry overwrite cancelled");
        StorePasswordEntryRequest request{
            EntryMutationMode::kUpdate,
            parts[1],
            "",
            ReadSecret("Entry password: "),
            ReadLine("Note: ")
        };
        auto request_guard = MakeScopedCleanse(request);
        const StorePasswordEntryResult result = session.StoreEntry(request);
        std::cout << "updated " << result.entry_path << '\n';
        return;
    }

    if (command == "delete") {
        RequireArgumentCount(parts, 2, "delete <name>");
        RequireExactConfirmation(
            "Type the entry name to confirm deletion: ",
            parts[1],
            "entry deletion cancelled");
        const RemovePasswordEntryResult result = session.RemoveEntry(parts[1]);
        std::cout << "deleted " << result.entry_path << '\n';
        return;
    }

    if (command == "change-master-password") {
        RequireArgumentCount(parts, 1, "change-master-password");
        RequireExactConfirmation(
            "Type CHANGE to confirm master password rotation: ",
            "CHANGE",
            "master password rotation cancelled");
        std::string new_master_password = ReadConfirmedSecret(
            "New master password: ",
            "Confirm new master password: ",
            "new master passwords do not match");
        auto new_master_password_guard = MakeScopedCleanse(new_master_password);
        const RotateMasterPasswordResult result =
            session.RotateMasterPassword(new_master_password);
        std::cout << "updated " << result.master_key_path << '\n';
        return;
    }

    if (command == "quit" || command == "exit") {
        RequireArgumentCount(parts, 1, "quit");
        should_quit = true;
        return;
    }

    throw std::runtime_error("unknown shell command");
}

}  // namespace

int RunInteractiveShell() {
    VaultSession session = OpenOrInitializeSession();

    std::cout << "shell ready; type help for commands\n";

    std::string line;
    while (true) {
        if (!TryReadLine("zkvault> ", line)) {
            std::cout << '\n';
            return 0;
        }

        const std::vector<std::string> parts = SplitCommand(line);
        if (parts.empty()) {
            continue;
        }

        bool should_quit = false;
        try {
            ExecuteShellCommand(session, parts, should_quit);
        } catch (const std::exception& ex) {
            std::cout << "error: " << ex.what() << '\n';
        }

        if (should_quit) {
            return 0;
        }
    }
}
