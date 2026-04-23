#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "service/vault_runtime.hpp"

using ShellBrowseState = VaultBrowseState;
using ShellViewContext = VaultViewContext;
using ShellBrowseSnapshot = VaultBrowseSnapshot;
using ShellRuntimeState = VaultRuntimeState;
using OpenShellRuntimeResult = OpenVaultRuntimeResult;

std::optional<std::chrono::milliseconds> ReadShellIdleTimeout();

OpenShellRuntimeResult OpenOrInitializeShellRuntime();

FrontendActionResult ExecuteShellCommand(
    ShellRuntimeState& runtime,
    const FrontendCommand& command);

FrontendActionResult ExecutePreparedShellCommand(
    ShellRuntimeState& runtime,
    const FrontendCommand& command);

FrontendActionResult StoreShellEntryWithContent(
    ShellRuntimeState& runtime,
    EntryMutationMode mode,
    const std::string& entry_name,
    const std::string& password,
    const std::string& note);

FrontendActionResult RotateShellMasterPassword(
    ShellRuntimeState& runtime,
    const std::string& new_master_password);

FrontendActionResult RemoveShellEntryByName(
    ShellRuntimeState& runtime,
    const std::string& entry_name);

FrontendActionResult ShowCurrentShellBrowseView(
    ShellRuntimeState& runtime);

FrontendActionResult HandleShellIdleTimeout(ShellRuntimeState& runtime);

std::optional<FrontendActionResult> RecoverShellViewAfterFailure(
    ShellRuntimeState& runtime);

ShellBrowseSnapshot SnapshotShellBrowseState(const ShellRuntimeState& runtime);

bool ShellSessionUnlocked(const ShellRuntimeState& runtime) noexcept;
