#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "app/frontend_contract.hpp"
#include "crypto/secure_memory.hpp"
#include "shell/shell_runtime.hpp"

namespace tui_internal {

struct TuiPendingCommand {
    bool active = false;
    FrontendCommandKind kind = FrontendCommandKind::kHelp;
    std::string name;
};

inline void Cleanse(TuiPendingCommand& command) {
    ::Cleanse(command.name);
}

enum class TuiEntryFormField {
    kName,
    kPassword,
    kNote
};

struct TuiEntryFormState {
    bool active = false;
    EntryMutationMode mode = EntryMutationMode::kCreate;
    TuiEntryFormField field = TuiEntryFormField::kName;
    std::string name;
    std::string password;
    std::string note;
};

inline void Cleanse(TuiEntryFormState& state) {
    ::Cleanse(state.name);
    ::Cleanse(state.password);
    ::Cleanse(state.note);
}

enum class TuiMasterPasswordFormField {
    kNewPassword,
    kConfirmPassword
};

struct TuiMasterPasswordFormState {
    bool active = false;
    TuiMasterPasswordFormField field =
        TuiMasterPasswordFormField::kNewPassword;
    std::string new_master_password;
    std::string confirm_master_password;
};

inline void Cleanse(TuiMasterPasswordFormState& state) {
    ::Cleanse(state.new_master_password);
    ::Cleanse(state.confirm_master_password);
}

struct TuiBrowseFilterState {
    bool active = false;
    std::string term;
};

inline void Cleanse(TuiBrowseFilterState& state) {
    ::Cleanse(state.term);
}

inline void Cleanse(ExactConfirmationRule& rule) {
    ::Cleanse(rule.prompt);
    ::Cleanse(rule.expected_value);
    ::Cleanse(rule.mismatch_error);
}

struct TuiExactConfirmationState {
    bool active = false;
    FrontendCommandKind kind = FrontendCommandKind::kHelp;
    ExactConfirmationRule rule;
    std::string entry_name;
    std::string typed_value;
};

inline void Cleanse(TuiExactConfirmationState& state) {
    Cleanse(state.rule);
    ::Cleanse(state.entry_name);
    ::Cleanse(state.typed_value);
}

struct TuiRenderState {
    std::string status_message;
    TuiPendingCommand pending_command;
    TuiEntryFormState entry_form;
    TuiMasterPasswordFormState master_password_form;
    TuiBrowseFilterState browse_filter;
    TuiExactConfirmationState exact_confirmation;
};

inline void Cleanse(TuiRenderState& state) {
    ::Cleanse(state.status_message);
    Cleanse(state.pending_command);
    Cleanse(state.entry_form);
    Cleanse(state.master_password_form);
    Cleanse(state.browse_filter);
    Cleanse(state.exact_confirmation);
}

enum class TuiKey {
    kUnknown,
    kMoveUp,
    kMoveDown,
    kShowSelection,
    kHelp,
    kAdd,
    kDelete,
    kBrowse,
    kNextField,
    kBackspace,
    kLock,
    kUnlock,
    kQuit,
    kCharacter
};

enum class TuiInputStatus {
    kKey,
    kEof,
    kTimedOut
};

struct TuiInputEvent {
    TuiInputStatus status = TuiInputStatus::kKey;
    TuiKey key = TuiKey::kUnknown;
    char text = '\0';
};

inline void ReplaceStatusMessage(
    TuiRenderState& state,
    std::string next_message) {
    ::Cleanse(state.status_message);
    state.status_message = std::move(next_message);
}

inline void ClearStatusMessage(TuiRenderState& state) {
    ::Cleanse(state.status_message);
    state.status_message.clear();
}

inline void ClearPendingCommand(TuiRenderState& state) {
    Cleanse(state.pending_command);
    state.pending_command.name.clear();
    state.pending_command.active = false;
    state.pending_command.kind = FrontendCommandKind::kHelp;
}

inline void ClearEntryForm(TuiRenderState& state) {
    Cleanse(state.entry_form);
    state.entry_form.name.clear();
    state.entry_form.password.clear();
    state.entry_form.note.clear();
    state.entry_form.active = false;
    state.entry_form.mode = EntryMutationMode::kCreate;
    state.entry_form.field = TuiEntryFormField::kName;
}

inline void ClearMasterPasswordForm(TuiRenderState& state) {
    Cleanse(state.master_password_form);
    state.master_password_form.active = false;
    state.master_password_form.field =
        TuiMasterPasswordFormField::kNewPassword;
    state.master_password_form.new_master_password.clear();
    state.master_password_form.confirm_master_password.clear();
}

inline void ClearBrowseFilterForm(TuiRenderState& state) {
    Cleanse(state.browse_filter);
    state.browse_filter.active = false;
    state.browse_filter.term.clear();
}

inline void ClearExactConfirmation(TuiRenderState& state) {
    Cleanse(state.exact_confirmation);
    state.exact_confirmation.active = false;
    state.exact_confirmation.kind = FrontendCommandKind::kHelp;
    state.exact_confirmation.rule = ExactConfirmationRule{};
    state.exact_confirmation.entry_name.clear();
    state.exact_confirmation.typed_value.clear();
}

inline void ReplacePendingCommand(
    TuiRenderState& state,
    const FrontendCommand& command) {
    ClearPendingCommand(state);
    state.pending_command.active = true;
    state.pending_command.kind = command.kind;
    state.pending_command.name = command.name;
}

inline void BeginEntryForm(
    TuiRenderState& state,
    EntryMutationMode mode,
    const std::string& entry_name) {
    ClearEntryForm(state);
    state.entry_form.active = true;
    state.entry_form.mode = mode;
    state.entry_form.field = mode == EntryMutationMode::kCreate
                                 ? TuiEntryFormField::kName
                                 : TuiEntryFormField::kPassword;
    state.entry_form.name = entry_name;
}

inline void PopulateEntryFormForUpdate(
    TuiRenderState& state,
    const PasswordEntry& entry) {
    BeginEntryForm(state, EntryMutationMode::kUpdate, entry.name);
    state.entry_form.password = entry.password;
    state.entry_form.note = entry.note;
}

inline void BeginMasterPasswordForm(TuiRenderState& state) {
    ClearMasterPasswordForm(state);
    state.master_password_form.active = true;
}

inline void BeginBrowseFilterForm(TuiRenderState& state) {
    ClearBrowseFilterForm(state);
    state.browse_filter.active = true;
}

inline void BeginExactConfirmation(
    TuiRenderState& state,
    FrontendCommandKind kind,
    const std::string& entry_name,
    ExactConfirmationRule rule) {
    ClearExactConfirmation(state);
    state.exact_confirmation.active = true;
    state.exact_confirmation.kind = kind;
    state.exact_confirmation.entry_name = entry_name;
    state.exact_confirmation.rule = std::move(rule);
}

inline bool ShouldPreviewPreparedCommand(
    const ShellRuntimeState& runtime,
    const FrontendCommand& command) {
    switch (command.kind) {
        case FrontendCommandKind::kUnlock:
            return !runtime.session.has_value();
        case FrontendCommandKind::kAdd:
        case FrontendCommandKind::kUpdate:
        case FrontendCommandKind::kDelete:
        case FrontendCommandKind::kChangeMasterPassword:
            return runtime.session.has_value();
        default:
            return false;
    }
}

inline std::string BuildPendingStatusMessage(const FrontendCommand& command) {
    switch (command.kind) {
        case FrontendCommandKind::kUnlock:
            return "awaiting master password";
        case FrontendCommandKind::kAdd:
            return "collecting fields for entry " + command.name;
        case FrontendCommandKind::kUpdate:
            return "awaiting overwrite confirmation for " + command.name;
        case FrontendCommandKind::kDelete:
            return "awaiting deletion confirmation for " + command.name;
        case FrontendCommandKind::kChangeMasterPassword:
            return "awaiting master password rotation confirmation";
        default:
            return "";
    }
}

inline std::optional<std::string> SelectedBrowseEntryName(
    const ShellRuntimeState& runtime) {
    ShellBrowseSnapshot snapshot = SnapshotShellBrowseState(runtime);
    auto snapshot_guard = MakeScopedCleanse(snapshot);
    if (snapshot.selected_name.empty()) {
        return std::nullopt;
    }

    return snapshot.selected_name;
}

inline std::string& ActiveEntryFormFieldValue(TuiEntryFormState& state) {
    switch (state.field) {
        case TuiEntryFormField::kName:
            return state.name;
        case TuiEntryFormField::kPassword:
            return state.password;
        case TuiEntryFormField::kNote:
            return state.note;
    }

    throw std::runtime_error("unsupported tui entry form field");
}

inline std::string& ActiveMasterPasswordFormFieldValue(
    TuiMasterPasswordFormState& state) {
    switch (state.field) {
        case TuiMasterPasswordFormField::kNewPassword:
            return state.new_master_password;
        case TuiMasterPasswordFormField::kConfirmPassword:
            return state.confirm_master_password;
    }

    throw std::runtime_error("unsupported tui master password form field");
}

inline std::string& ActiveBrowseFilterFieldValue(
    TuiBrowseFilterState& state) {
    return state.term;
}

inline void AdvanceEntryFormField(TuiEntryFormState& state) {
    if (state.field == TuiEntryFormField::kName) {
        state.field = TuiEntryFormField::kPassword;
        return;
    }

    if (state.field == TuiEntryFormField::kPassword) {
        state.field = TuiEntryFormField::kNote;
    }
}

inline void AdvanceMasterPasswordFormField(
    TuiMasterPasswordFormState& state) {
    if (state.field == TuiMasterPasswordFormField::kNewPassword) {
        state.field = TuiMasterPasswordFormField::kConfirmPassword;
    }
}

inline std::string MaskSecret(std::string_view value) {
    return std::string(value.size(), '*');
}

inline void EraseLastCharacter(std::string& value) {
    if (value.empty()) {
        return;
    }

    value.back() = '\0';
    value.pop_back();
}

inline std::string RenderTuiStatusMessage(
    const FrontendActionResult& result) {
    if (result.state == FrontendSessionState::kShowingHelp ||
        result.state == FrontendSessionState::kShowingList ||
        result.state == FrontendSessionState::kShowingEntry ||
        result.payload_kind == FrontendPayloadKind::kNone) {
        return "";
    }

    return RenderFrontendActionResult(result);
}

}  // namespace tui_internal
