#include "shell/shell_runtime.hpp"

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "crypto/secure_memory.hpp"
#include "service/vault_runtime.hpp"
#include "terminal/prompt.hpp"

namespace {

constexpr const char* kShellIdleTimeoutEnv =
    "ZKVAULT_SHELL_IDLE_TIMEOUT_SECONDS";

}  // namespace

std::optional<std::chrono::milliseconds> ReadShellIdleTimeout() {
    const char* raw_value = std::getenv(kShellIdleTimeoutEnv);
    if (raw_value == nullptr || *raw_value == '\0') {
        return std::nullopt;
    }

    int seconds = 0;
    const char* parse_end = raw_value + std::char_traits<char>::length(raw_value);
    const auto [end, error] = std::from_chars(raw_value, parse_end, seconds);
    if (error != std::errc() || end != parse_end || seconds <= 0) {
        throw std::runtime_error(
            "invalid ZKVAULT_SHELL_IDLE_TIMEOUT_SECONDS");
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::seconds(seconds));
}

OpenShellRuntimeResult OpenOrInitializeShellRuntime() {
    if (!VaultIsInitialized()) {
        const std::string choice = ReadLine(
            "Vault not initialized. Create one now? [y/N]: ");
        if (choice != "y" && choice != "Y" && choice != "yes" && choice != "YES") {
            throw std::runtime_error("vault not initialized");
        }

        InitializeVaultRequest request{
            ReadConfirmedSecret(
                "Master password: ",
                "Confirm master password: ",
                "master passwords do not match")
        };
        auto request_guard = MakeScopedCleanse(request);
        return InitializeVaultRuntime(request);
    }

    std::string master_password = ReadSecret("Master password: ");
    auto master_password_guard = MakeScopedCleanse(master_password);
    return OpenVaultRuntime(master_password);
}

FrontendActionResult ExecuteShellCommand(
    ShellRuntimeState& runtime,
    const FrontendCommand& command) {
    static_cast<void>(runtime.state_machine.HandleCommand(command.kind));
    return ExecutePreparedShellCommand(runtime, command);
}

FrontendActionResult ExecutePreparedShellCommand(
    ShellRuntimeState& runtime,
    const FrontendCommand& command) {
    if (command.kind == FrontendCommandKind::kUnlock) {
        std::string master_password = ReadSecret("Master password: ");
        auto master_password_guard = MakeScopedCleanse(master_password);
        return UnlockVaultRuntime(runtime, master_password);
    }

    if (command.kind == FrontendCommandKind::kAdd) {
        std::string password = ReadSecret("Entry password: ");
        auto password_guard = MakeScopedCleanse(password);
        std::string note = ReadLine("Note: ");
        auto note_guard = MakeScopedCleanse(note);
        return StoreShellEntryWithContent(
            runtime,
            EntryMutationMode::kCreate,
            command.name,
            password,
            note);
    }

    if (command.kind == FrontendCommandKind::kUpdate) {
        const ExactConfirmationRule rule =
            BuildOverwriteConfirmationRule(command.name);
        RequireExactConfirmation(
            rule.prompt,
            rule.expected_value,
            rule.mismatch_error);
        static_cast<void>(runtime.state_machine.HandleConfirmationAccepted());
        std::string password = ReadSecret("Entry password: ");
        auto password_guard = MakeScopedCleanse(password);
        std::string note = ReadLine("Note: ");
        auto note_guard = MakeScopedCleanse(note);
        return StoreShellEntryWithContent(
            runtime,
            EntryMutationMode::kUpdate,
            command.name,
            password,
            note);
    }

    if (command.kind == FrontendCommandKind::kDelete) {
        const ExactConfirmationRule rule =
            BuildDeletionConfirmationRule(command.name);
        RequireExactConfirmation(
            rule.prompt,
            rule.expected_value,
            rule.mismatch_error);
        static_cast<void>(runtime.state_machine.HandleConfirmationAccepted());
        return RemoveShellEntryByName(runtime, command.name);
    }

    if (command.kind == FrontendCommandKind::kChangeMasterPassword) {
        const ExactConfirmationRule rule =
            BuildMasterPasswordRotationConfirmationRule();
        RequireExactConfirmation(
            rule.prompt,
            rule.expected_value,
            rule.mismatch_error);
        static_cast<void>(runtime.state_machine.HandleConfirmationAccepted());
        std::string new_master_password = ReadConfirmedSecret(
            "New master password: ",
            "Confirm new master password: ",
            "new master passwords do not match");
        auto new_master_password_guard = MakeScopedCleanse(new_master_password);
        return RotateShellMasterPassword(runtime, new_master_password);
    }

    return ExecutePreparedVaultCommand(runtime, command);
}

FrontendActionResult StoreShellEntryWithContent(
    ShellRuntimeState& runtime,
    EntryMutationMode mode,
    const std::string& entry_name,
    const std::string& password,
    const std::string& note) {
    return StoreVaultEntryWithContent(
        runtime,
        mode,
        entry_name,
        password,
        note);
}

FrontendActionResult RotateShellMasterPassword(
    ShellRuntimeState& runtime,
    const std::string& new_master_password) {
    return RotateVaultMasterPassword(runtime, new_master_password);
}

FrontendActionResult RemoveShellEntryByName(
    ShellRuntimeState& runtime,
    const std::string& entry_name) {
    return RemoveVaultEntryByName(runtime, entry_name);
}

FrontendActionResult ShowCurrentShellBrowseView(
    ShellRuntimeState& runtime) {
    return ShowCurrentVaultBrowseView(runtime);
}

FrontendActionResult HandleShellIdleTimeout(ShellRuntimeState& runtime) {
    return HandleVaultIdleTimeout(runtime);
}

std::optional<FrontendActionResult> RecoverShellViewAfterFailure(
    ShellRuntimeState& runtime) {
    return RecoverVaultViewAfterFailure(runtime);
}

ShellBrowseSnapshot SnapshotShellBrowseState(const ShellRuntimeState& runtime) {
    return SnapshotVaultBrowseState(runtime);
}

bool ShellSessionUnlocked(const ShellRuntimeState& runtime) noexcept {
    return VaultSessionUnlocked(runtime);
}
