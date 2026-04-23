#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "app/frontend_contract.hpp"
#include "app/frontend_output.hpp"
#include "app/vault_app.hpp"
#include "content/private_post.hpp"
#include "content/private_post_file.hpp"
#include "crypto/secure_memory.hpp"
#include "model/password_entry.hpp"
#include "shell/interactive_shell.hpp"
#include "tui/terminal_ui.hpp"
#include "terminal/prompt.hpp"

namespace {

int PrintCliUsageAndReturn() {
    PrintFrontendResult(std::cout, BuildCliUsageResult());
    return 1;
}

void RequireEntryExistsOrThrow(const char* entry_name) {
    if (!EntryExists(entry_name)) {
        throw std::runtime_error("entry does not exist");
    }
}

void ValidateStoreCommandTarget(
    std::string_view command,
    const char* entry_name) {
    const bool entry_exists = EntryExists(entry_name);
    if (command == "add" && entry_exists) {
        throw std::runtime_error("entry already exists");
    }

    if (command == "update" && !entry_exists) {
        throw std::runtime_error("entry does not exist");
    }
}

int RunShellCommand(int argc) {
    if (argc != 2) {
        return PrintCliUsageAndReturn();
    }

    return RunInteractiveShell();
}

int RunTuiCommand(int argc) {
    if (argc != 2) {
        return PrintCliUsageAndReturn();
    }

    return RunTerminalUi();
}

int RunInitCommand(int argc) {
    if (argc != 2) {
        return PrintCliUsageAndReturn();
    }

    InitializeVaultRequest request{
        ReadConfirmedSecret(
            "Master password: ",
            "Confirm master password: ",
            "master passwords do not match")
    };
    auto request_guard = MakeScopedCleanse(request);
    const InitializeVaultResult result = InitializeVault(request);
    PrintFrontendResult(
        std::cout,
        BuildInitializedResult(result.master_key_path));
    return 0;
}

int RunChangeMasterPasswordCommand(int argc) {
    if (argc != 2) {
        return PrintCliUsageAndReturn();
    }

    const ExactConfirmationRule rule =
        BuildMasterPasswordRotationConfirmationRule();
    RequireExactConfirmation(
        rule.prompt,
        rule.expected_value,
        rule.mismatch_error);
    RotateMasterPasswordRequest request{
        ReadSecret("Current master password: "),
        ReadConfirmedSecret(
            "New master password: ",
            "Confirm new master password: ",
            "new master passwords do not match")
    };
    auto request_guard = MakeScopedCleanse(request);
    const RotateMasterPasswordResult result = RotateMasterPassword(request);
    PrintFrontendResult(
        std::cout,
        BuildUpdatedResult(result.master_key_path));
    return 0;
}

int RunStoreCommand(
    std::string_view command,
    int argc,
    char* argv[]) {
    if (argc != 3) {
        return PrintCliUsageAndReturn();
    }

    const bool is_add = command == "add";
    const char* entry_name = argv[2];
    ValidateStoreCommandTarget(command, entry_name);

    if (!is_add) {
        const ExactConfirmationRule rule =
            BuildOverwriteConfirmationRule(entry_name);
        RequireExactConfirmation(
            rule.prompt,
            rule.expected_value,
            rule.mismatch_error);
    }

    StorePasswordEntryRequest request{
        is_add ? EntryMutationMode::kCreate
               : EntryMutationMode::kUpdate,
        entry_name,
        ReadSecret("Master password: "),
        ReadSecret("Entry password: "),
        ReadLine("Note: ")
    };
    auto request_guard = MakeScopedCleanse(request);
    const StorePasswordEntryResult result = StorePasswordEntry(request);
    PrintFrontendResult(
        std::cout,
        is_add
            ? BuildStoredEntryResult(result.entry_path)
            : BuildUpdatedResult(result.entry_path));
    return 0;
}

int RunGetCommand(int argc, char* argv[]) {
    if (argc != 3) {
        return PrintCliUsageAndReturn();
    }

    const char* entry_name = argv[2];
    RequireEntryExistsOrThrow(entry_name);

    LoadPasswordEntryRequest request{
        entry_name,
        ReadSecret("Master password: ")
    };
    auto request_guard = MakeScopedCleanse(request);
    PasswordEntry entry = LoadPasswordEntry(request);
    auto entry_guard = MakeScopedCleanse(entry);
    PrintFrontendResult(
        std::cout,
        BuildShowEntryResult(std::move(entry)));
    return 0;
}

int RunDeleteCommand(int argc, char* argv[]) {
    if (argc != 3) {
        return PrintCliUsageAndReturn();
    }

    const char* entry_name = argv[2];
    RequireEntryExistsOrThrow(entry_name);

    const ExactConfirmationRule rule =
        BuildDeletionConfirmationRule(entry_name);
    RequireExactConfirmation(
        rule.prompt,
        rule.expected_value,
        rule.mismatch_error);
    const RemovePasswordEntryResult result =
        RemovePasswordEntry(RemovePasswordEntryRequest{entry_name});
    PrintFrontendResult(
        std::cout,
        BuildDeletedEntryResult(result.entry_path));
    return 0;
}

int RunListCommand(int argc) {
    if (argc != 2) {
        return PrintCliUsageAndReturn();
    }

    PrintFrontendResult(
        std::cout,
        BuildListResult(ListEntryNames(), ""));
    return 0;
}

int RunEncryptPostCommand(int argc, char* argv[]) {
    if (argc != 4) {
        return PrintCliUsageAndReturn();
    }

    const std::filesystem::path document_path = argv[2];
    const std::filesystem::path bundle_path = argv[3];
    PrivatePostDocument document = LoadPrivatePostDocumentFile(document_path);
    auto document_guard = MakeScopedCleanse(document);
    std::string access_password = ReadConfirmedSecret(
        "Access password: ",
        "Confirm access password: ",
        "access passwords do not match");
    auto access_password_guard = MakeScopedCleanse(access_password);
    EncryptedPrivatePostBundle bundle =
        EncryptPrivatePostDocument(document, access_password);
    auto bundle_guard = MakeScopedCleanse(bundle);
    SaveEncryptedPrivatePostBundleFile(bundle_path, bundle);
    std::cout << "encrypted private post saved to " << bundle_path.string()
              << '\n';
    return 0;
}

int RunDecryptPostPreviewCommand(int argc, char* argv[]) {
    if (argc != 3) {
        return PrintCliUsageAndReturn();
    }

    const std::filesystem::path bundle_path = argv[2];
    EncryptedPrivatePostBundle bundle =
        LoadEncryptedPrivatePostBundleFile(bundle_path);
    auto bundle_guard = MakeScopedCleanse(bundle);
    std::string access_password = ReadSecret("Access password: ");
    auto access_password_guard = MakeScopedCleanse(access_password);
    PrivatePostDocument document =
        DecryptPrivatePostDocument(bundle, access_password);
    auto document_guard = MakeScopedCleanse(document);
    std::string output = nlohmann::json(document).dump(2);
    auto output_guard = MakeScopedCleanse(output);
    std::cout << output << '\n';
    return 0;
}

int DispatchCommand(int argc, char* argv[]) {
    if (argc < 2) {
        return PrintCliUsageAndReturn();
    }

    const std::string_view command = argv[1];
    if (command == "shell") {
        return RunShellCommand(argc);
    }

    if (command == "tui") {
        return RunTuiCommand(argc);
    }

    if (command == "init") {
        return RunInitCommand(argc);
    }

    if (command == "change-master-password") {
        return RunChangeMasterPasswordCommand(argc);
    }

    if (command == "add" || command == "update") {
        return RunStoreCommand(command, argc, argv);
    }

    if (command == "get") {
        return RunGetCommand(argc, argv);
    }

    if (command == "delete") {
        return RunDeleteCommand(argc, argv);
    }

    if (command == "list") {
        return RunListCommand(argc);
    }

    if (command == "encrypt-post") {
        return RunEncryptPostCommand(argc, argv);
    }

    if (command == "decrypt-post-preview") {
        return RunDecryptPostPreviewCommand(argc, argv);
    }

    return PrintCliUsageAndReturn();
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        return DispatchCommand(argc, argv);
    } catch (const std::exception& ex) {
        PrintFrontendError(std::cerr, ex.what());
        return 1;
    }
}
