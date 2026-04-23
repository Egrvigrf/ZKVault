#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app/frontend_contract.hpp"
#include "app/vault_app.hpp"
#include "app/vault_session.hpp"
#include "crypto/secure_memory.hpp"

struct VaultBrowseState {
    bool active = false;
    std::string filter_term;
    std::vector<std::string> visible_entry_names;
    std::size_t selected_index = 0;
};

inline void Cleanse(VaultBrowseState& state) {
    Cleanse(state.filter_term);
    Cleanse(state.visible_entry_names);
}

struct VaultViewContext {
    std::string entry_name;
};

inline void Cleanse(VaultViewContext& context) {
    Cleanse(context.entry_name);
}

struct VaultBrowseSnapshot {
    bool active = false;
    std::string filter_term;
    std::string selected_name;
    std::vector<std::string> entry_names;
    std::string empty_message;
};

inline void Cleanse(VaultBrowseSnapshot& snapshot) {
    Cleanse(snapshot.filter_term);
    Cleanse(snapshot.selected_name);
    Cleanse(snapshot.entry_names);
    Cleanse(snapshot.empty_message);
}

struct VaultRuntimeState {
    FrontendStateMachine state_machine;
    std::optional<VaultSession> session;
    VaultBrowseState browse_state;
    VaultViewContext view_context;
};

inline void Cleanse(VaultRuntimeState& runtime) {
    Cleanse(runtime.browse_state);
    Cleanse(runtime.view_context);
}

struct OpenVaultRuntimeResult {
    VaultRuntimeState runtime;
    std::optional<FrontendActionResult> startup_result;
};

inline void Cleanse(OpenVaultRuntimeResult& result) {
    Cleanse(result.runtime);
    if (result.startup_result.has_value()) {
        Cleanse(*result.startup_result);
    }
}

bool VaultIsInitialized();

OpenVaultRuntimeResult OpenVaultRuntime(const std::string& master_password);

OpenVaultRuntimeResult InitializeVaultRuntime(
    const InitializeVaultRequest& request);

FrontendActionResult ExecuteVaultCommand(
    VaultRuntimeState& runtime,
    const FrontendCommand& command);

FrontendActionResult ExecutePreparedVaultCommand(
    VaultRuntimeState& runtime,
    const FrontendCommand& command);

FrontendActionResult UnlockVaultRuntime(
    VaultRuntimeState& runtime,
    const std::string& master_password);

FrontendActionResult StoreVaultEntryWithContent(
    VaultRuntimeState& runtime,
    EntryMutationMode mode,
    const std::string& entry_name,
    const std::string& password,
    const std::string& note);

FrontendActionResult RotateVaultMasterPassword(
    VaultRuntimeState& runtime,
    const std::string& new_master_password);

FrontendActionResult RemoveVaultEntryByName(
    VaultRuntimeState& runtime,
    const std::string& entry_name);

FrontendActionResult ShowCurrentVaultBrowseView(VaultRuntimeState& runtime);

FrontendActionResult HandleVaultIdleTimeout(VaultRuntimeState& runtime);

std::optional<FrontendActionResult> RecoverVaultViewAfterFailure(
    VaultRuntimeState& runtime);

VaultBrowseSnapshot SnapshotVaultBrowseState(const VaultRuntimeState& runtime);

bool VaultSessionUnlocked(const VaultRuntimeState& runtime) noexcept;
