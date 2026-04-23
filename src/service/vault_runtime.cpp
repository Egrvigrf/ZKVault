#include "service/vault_runtime.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

char LowerAscii(char ch) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }

    return static_cast<char>(value);
}

bool ContainsCaseInsensitive(std::string_view value, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }

    if (needle.size() > value.size()) {
        return false;
    }

    for (std::size_t offset = 0; offset + needle.size() <= value.size();
         ++offset) {
        bool matches = true;
        for (std::size_t i = 0; i < needle.size(); ++i) {
            if (LowerAscii(value[offset + i]) != LowerAscii(needle[i])) {
                matches = false;
                break;
            }
        }

        if (matches) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> FilterEntryNames(
    const std::vector<std::string>& entry_names,
    std::string_view query) {
    std::vector<std::string> matches;
    for (const std::string& entry_name : entry_names) {
        if (ContainsCaseInsensitive(entry_name, query)) {
            matches.push_back(entry_name);
        }
    }

    return matches;
}

void ResetBrowseState(VaultBrowseState& state) {
    Cleanse(state);
    state.active = false;
    state.filter_term.clear();
    state.visible_entry_names.clear();
    state.selected_index = 0;
}

bool HasBrowseSelection(const VaultBrowseState& state) {
    return state.active &&
           state.selected_index < state.visible_entry_names.size();
}

const std::string& SelectedBrowseEntryName(const VaultBrowseState& state) {
    return state.visible_entry_names[state.selected_index];
}

std::string BrowseEmptyMessage(const VaultBrowseState& state) {
    if (state.active && !state.filter_term.empty()) {
        return "(no matches)";
    }

    return "(empty)";
}

void ReplaceVisibleEntryNames(
    VaultBrowseState& state,
    std::vector<std::string> entry_names) {
    Cleanse(state.visible_entry_names);
    state.visible_entry_names = std::move(entry_names);
}

bool SelectBrowseEntry(
    VaultBrowseState& state,
    std::string_view entry_name) {
    if (!state.active) {
        return false;
    }

    for (std::size_t i = 0; i < state.visible_entry_names.size(); ++i) {
        if (state.visible_entry_names[i] == entry_name) {
            state.selected_index = i;
            return true;
        }
    }

    return false;
}

void ActivateBrowseState(
    VaultSession& session,
    VaultBrowseState& state,
    std::string_view filter_term) {
    state.active = true;
    Cleanse(state.filter_term);
    state.filter_term = std::string(filter_term);
    ReplaceVisibleEntryNames(
        state,
        FilterEntryNames(session.ListEntryNames(), filter_term));
    state.selected_index = 0;
}

void EnsureBrowseStateActive(
    VaultSession& session,
    VaultBrowseState& state,
    bool select_last) {
    if (!state.active) {
        ActivateBrowseState(session, state, "");
        if (select_last && !state.visible_entry_names.empty()) {
            state.selected_index = state.visible_entry_names.size() - 1;
        }
    }
}

void StepBrowseSelection(
    VaultSession& session,
    VaultBrowseState& state,
    bool move_forward) {
    EnsureBrowseStateActive(session, state, !move_forward);
    if (state.visible_entry_names.empty()) {
        return;
    }

    if (move_forward) {
        state.selected_index =
            (state.selected_index + 1) % state.visible_entry_names.size();
        return;
    }

    state.selected_index =
        (state.selected_index + state.visible_entry_names.size() - 1) %
        state.visible_entry_names.size();
}

void RefreshBrowseState(VaultSession& session, VaultBrowseState& state) {
    if (!state.active) {
        return;
    }

    const std::string previously_selected =
        HasBrowseSelection(state) ? SelectedBrowseEntryName(state) : "";
    const std::size_t previous_index = state.selected_index;
    ReplaceVisibleEntryNames(
        state,
        FilterEntryNames(session.ListEntryNames(), state.filter_term));
    if (state.visible_entry_names.empty()) {
        state.selected_index = 0;
        return;
    }

    if (!previously_selected.empty() &&
        SelectBrowseEntry(state, previously_selected)) {
        return;
    }

    if (previous_index >= state.visible_entry_names.size()) {
        state.selected_index = state.visible_entry_names.size() - 1;
        return;
    }

    state.selected_index = previous_index;
}

void SyncBrowseStateAfterMutation(
    VaultSession& session,
    VaultBrowseState& state,
    std::string_view entry_name) {
    if (!state.active) {
        ActivateBrowseState(session, state, "");
    } else {
        RefreshBrowseState(session, state);
    }

    if (entry_name.empty()) {
        return;
    }

    static_cast<void>(SelectBrowseEntry(state, entry_name));
}

void FocusBrowseEntry(
    VaultSession& session,
    VaultBrowseState& state,
    std::string_view entry_name) {
    if (SelectBrowseEntry(state, entry_name)) {
        return;
    }

    ActivateBrowseState(session, state, "");
    static_cast<void>(SelectBrowseEntry(state, entry_name));
}

FrontendActionResult BuildBrowseResult(const VaultBrowseState& state) {
    return BuildFocusedListResult(
        state.visible_entry_names,
        HasBrowseSelection(state) ? SelectedBrowseEntryName(state) : "",
        state.filter_term,
        BrowseEmptyMessage(state));
}

void RememberVaultViewContext(
    const FrontendActionResult& result,
    VaultViewContext& context) {
    if (result.state == FrontendSessionState::kShowingEntry) {
        Cleanse(context.entry_name);
        context.entry_name = result.entry.name;
        return;
    }

    Cleanse(context.entry_name);
    context.entry_name.clear();
}

std::optional<FrontendActionResult> BuildRecoveredViewResult(
    const std::optional<VaultSession>& session,
    const VaultBrowseState& browse_state,
    const VaultViewContext& view_context,
    FrontendSessionState recovered_state) {
    if (recovered_state == FrontendSessionState::kShowingHelp) {
        return BuildShellHelpResult();
    }

    if (recovered_state == FrontendSessionState::kShowingList) {
        return BuildBrowseResult(browse_state);
    }

    if (recovered_state != FrontendSessionState::kShowingEntry ||
        !session.has_value() ||
        view_context.entry_name.empty()) {
        return std::nullopt;
    }

    PasswordEntry entry = session->LoadEntry(view_context.entry_name);
    auto entry_guard = MakeScopedCleanse(entry);
    return BuildShowEntryResult(std::move(entry));
}

FrontendActionResult FinalizeVaultResult(
    FrontendStateMachine& state_machine,
    VaultViewContext& view_context,
    FrontendActionResult result) {
    static_cast<void>(state_machine.ApplyActionResult(result));
    RememberVaultViewContext(result, view_context);
    return result;
}

}  // namespace

bool VaultIsInitialized() {
    return std::filesystem::exists(".zkv_master");
}

OpenVaultRuntimeResult OpenVaultRuntime(const std::string& master_password) {
    OpenVaultRuntimeResult result;
    const FrontendSessionState state =
        result.runtime.state_machine.HandleStartup(VaultIsInitialized());
    if (state == FrontendSessionState::kInitializingVault) {
        throw std::runtime_error("vault not initialized");
    }

    result.runtime.session.emplace(VaultSession::Open(master_password));
    return result;
}

OpenVaultRuntimeResult InitializeVaultRuntime(
    const InitializeVaultRequest& request) {
    OpenVaultRuntimeResult result;
    const FrontendSessionState state =
        result.runtime.state_machine.HandleStartup(VaultIsInitialized());
    if (state != FrontendSessionState::kInitializingVault) {
        throw std::runtime_error("vault already initialized");
    }

    const InitializeVaultResult init_result = InitializeVault(request);
    result.startup_result = BuildInitializedResult(init_result.master_key_path);
    static_cast<void>(result.runtime.state_machine.ApplyActionResult(
        *result.startup_result));
    result.runtime.session.emplace(VaultSession::Open(request.master_password));
    return result;
}

FrontendActionResult ExecuteVaultCommand(
    VaultRuntimeState& runtime,
    const FrontendCommand& command) {
    static_cast<void>(runtime.state_machine.HandleCommand(command.kind));
    return ExecutePreparedVaultCommand(runtime, command);
}

FrontendActionResult ExecutePreparedVaultCommand(
    VaultRuntimeState& runtime,
    const FrontendCommand& command) {
    if (command.kind == FrontendCommandKind::kHelp) {
        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            BuildShellHelpResult());
    }

    if (command.kind == FrontendCommandKind::kLock) {
        if (!runtime.session.has_value()) {
            throw std::runtime_error("vault is already locked");
        }

        runtime.session.reset();
        ResetBrowseState(runtime.browse_state);
        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            BuildLockedResult());
    }

    if (command.kind == FrontendCommandKind::kQuit) {
        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            BuildQuitResult());
    }

    if (command.kind == FrontendCommandKind::kUnlock) {
        throw std::runtime_error("unlock requires master password");
    }

    if (!runtime.session.has_value()) {
        throw std::runtime_error("vault is locked");
    }

    VaultSession& active_session = *runtime.session;

    if (command.kind == FrontendCommandKind::kList) {
        ActivateBrowseState(active_session, runtime.browse_state, "");
        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            BuildBrowseResult(runtime.browse_state));
    }

    if (command.kind == FrontendCommandKind::kFind) {
        ActivateBrowseState(active_session, runtime.browse_state, command.name);
        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            BuildBrowseResult(runtime.browse_state));
    }

    if (command.kind == FrontendCommandKind::kNext) {
        StepBrowseSelection(active_session, runtime.browse_state, true);
        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            BuildBrowseResult(runtime.browse_state));
    }

    if (command.kind == FrontendCommandKind::kPrev) {
        StepBrowseSelection(active_session, runtime.browse_state, false);
        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            BuildBrowseResult(runtime.browse_state));
    }

    if (command.kind == FrontendCommandKind::kShow) {
        const std::string entry_name = command.name.empty()
                                           ? (HasBrowseSelection(runtime.browse_state)
                                                  ? SelectedBrowseEntryName(
                                                        runtime.browse_state)
                                                  : "")
                                           : command.name;
        if (entry_name.empty()) {
            throw std::runtime_error("no entry selected");
        }

        PasswordEntry entry = active_session.LoadEntry(entry_name);
        auto entry_guard = MakeScopedCleanse(entry);
        FocusBrowseEntry(active_session, runtime.browse_state, entry_name);
        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            BuildShowEntryResult(std::move(entry)));
    }

    throw std::runtime_error("command requires explicit input");
}

FrontendActionResult UnlockVaultRuntime(
    VaultRuntimeState& runtime,
    const std::string& master_password) {
    if (runtime.session.has_value()) {
        throw std::runtime_error("vault is already unlocked");
    }

    runtime.session.emplace(VaultSession::Open(master_password));
    ResetBrowseState(runtime.browse_state);
    return FinalizeVaultResult(
        runtime.state_machine,
        runtime.view_context,
        BuildUnlockedResult());
}

FrontendActionResult StoreVaultEntryWithContent(
    VaultRuntimeState& runtime,
    EntryMutationMode mode,
    const std::string& entry_name,
    const std::string& password,
    const std::string& note) {
    if (!runtime.session.has_value()) {
        throw std::runtime_error("vault is locked");
    }

    VaultSession& active_session = *runtime.session;
    StorePasswordEntryRequest request{
        mode,
        entry_name,
        "",
        password,
        note
    };
    auto request_guard = MakeScopedCleanse(request);
    const StorePasswordEntryResult result = active_session.StoreEntry(request);
    SyncBrowseStateAfterMutation(
        active_session,
        runtime.browse_state,
        entry_name);
    return FinalizeVaultResult(
        runtime.state_machine,
        runtime.view_context,
        mode == EntryMutationMode::kCreate
            ? BuildStoredEntryResult(result.entry_path)
            : BuildUpdatedResult(result.entry_path));
}

FrontendActionResult RotateVaultMasterPassword(
    VaultRuntimeState& runtime,
    const std::string& new_master_password) {
    if (!runtime.session.has_value()) {
        throw std::runtime_error("vault is locked");
    }

    VaultSession& active_session = *runtime.session;
    const RotateMasterPasswordResult result =
        active_session.RotateMasterPassword(new_master_password);
    return FinalizeVaultResult(
        runtime.state_machine,
        runtime.view_context,
        BuildUpdatedResult(result.master_key_path));
}

FrontendActionResult RemoveVaultEntryByName(
    VaultRuntimeState& runtime,
    const std::string& entry_name) {
    if (!runtime.session.has_value()) {
        throw std::runtime_error("vault is locked");
    }

    VaultSession& active_session = *runtime.session;
    const RemovePasswordEntryResult result =
        active_session.RemoveEntry(entry_name);
    RefreshBrowseState(active_session, runtime.browse_state);
    return FinalizeVaultResult(
        runtime.state_machine,
        runtime.view_context,
        BuildDeletedEntryResult(result.entry_path));
}

FrontendActionResult ShowCurrentVaultBrowseView(VaultRuntimeState& runtime) {
    if (!runtime.session.has_value()) {
        throw std::runtime_error("vault is locked");
    }

    VaultSession& active_session = *runtime.session;
    if (!runtime.browse_state.active) {
        ActivateBrowseState(active_session, runtime.browse_state, "");
    }

    return FinalizeVaultResult(
        runtime.state_machine,
        runtime.view_context,
        BuildBrowseResult(runtime.browse_state));
}

FrontendActionResult HandleVaultIdleTimeout(VaultRuntimeState& runtime) {
    if (!runtime.session.has_value()) {
        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            BuildLockedResult());
    }

    static_cast<void>(runtime.state_machine.HandleIdleTimeout());
    runtime.session.reset();
    ResetBrowseState(runtime.browse_state);
    return FinalizeVaultResult(
        runtime.state_machine,
        runtime.view_context,
        BuildIdleLockedResult());
}

std::optional<FrontendActionResult> RecoverVaultViewAfterFailure(
    VaultRuntimeState& runtime) {
    const FrontendSessionState recovered_state =
        runtime.state_machine.HandleFailure(runtime.session.has_value());
    try {
        std::optional<FrontendActionResult> recovered_result =
            BuildRecoveredViewResult(
                runtime.session,
                runtime.browse_state,
                runtime.view_context,
                recovered_state);
        if (!recovered_result.has_value()) {
            return std::nullopt;
        }

        return FinalizeVaultResult(
            runtime.state_machine,
            runtime.view_context,
            std::move(*recovered_result));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

VaultBrowseSnapshot SnapshotVaultBrowseState(const VaultRuntimeState& runtime) {
    if (!runtime.session.has_value()) {
        return VaultBrowseSnapshot{
            false,
            "",
            "",
            {},
            "unlock to browse entries"
        };
    }

    if (!runtime.browse_state.active) {
        return VaultBrowseSnapshot{
            false,
            "",
            "",
            runtime.session->ListEntryNames(),
            "(empty)"
        };
    }

    return VaultBrowseSnapshot{
        true,
        runtime.browse_state.filter_term,
        HasBrowseSelection(runtime.browse_state)
            ? SelectedBrowseEntryName(runtime.browse_state)
            : "",
        runtime.browse_state.visible_entry_names,
        BrowseEmptyMessage(runtime.browse_state)
    };
}

bool VaultSessionUnlocked(const VaultRuntimeState& runtime) noexcept {
    return runtime.session.has_value();
}
