#include "tui/terminal_ui_render.hpp"

#include <iostream>
#include <string_view>

#include "terminal/display.hpp"

namespace tui_internal {
namespace {

void RenderBrowseSnapshot(const ShellBrowseSnapshot& snapshot) {
    std::cout << "Entries";
    if (!snapshot.filter_term.empty()) {
        std::cout << " [filter: " << snapshot.filter_term << "]";
    }
    std::cout << ":\n";

    if (snapshot.entry_names.empty()) {
        std::cout << snapshot.empty_message << '\n';
        return;
    }

    for (const std::string& entry_name : snapshot.entry_names) {
        const bool is_selected =
            !snapshot.selected_name.empty() &&
            entry_name == snapshot.selected_name;
        std::cout << (is_selected ? "> " : "  ") << entry_name << '\n';
    }
}

std::string_view DescribeCurrentView(const ShellRuntimeState& runtime) {
    switch (runtime.state_machine.state()) {
        case FrontendSessionState::kShowingHelp:
            return "help";
        case FrontendSessionState::kShowingList:
            return "list";
        case FrontendSessionState::kShowingEntry:
            return "entry";
        case FrontendSessionState::kLocked:
            return "locked";
        case FrontendSessionState::kReady:
            return "ready";
        case FrontendSessionState::kUnlockingSession:
            return "unlock";
        case FrontendSessionState::kEditingEntryForm:
            return "edit-entry";
        case FrontendSessionState::kEditingMasterPasswordForm:
            return "edit-master-password";
        case FrontendSessionState::kConfirmingEntryOverwrite:
            return "confirm-overwrite";
        case FrontendSessionState::kConfirmingEntryDeletion:
            return "confirm-delete";
        case FrontendSessionState::kConfirmingMasterPasswordRotation:
            return "confirm-master-password-rotation";
        case FrontendSessionState::kRecoveringFromFailure:
            return "recovering";
        default:
            return "transient";
    }
}

std::string_view DescribeVisibleView(
    const ShellRuntimeState& runtime,
    const TuiRenderState& render_state) {
    if (render_state.browse_filter.active) {
        return "filter";
    }

    return DescribeCurrentView(runtime);
}

void RenderStatusSection(std::string_view status_message) {
    std::cout << "Status:\n";
    if (status_message.empty()) {
        std::cout << "(none)\n";
        return;
    }

    std::cout << status_message << '\n';
}

void RenderHelpView() {
    std::cout << "Keys:\n";
    std::cout << "  Down / j  move selection forward\n";
    std::cout << "  Up / k    move selection backward\n";
    std::cout << "  Enter     view the selected entry\n";
    std::cout << "  f or /    filter entries\n";
    std::cout << "  a         create a new entry\n";
    std::cout << "  e         update the selected entry\n";
    std::cout << "  d         delete the selected entry\n";
    std::cout << "  m         change the master password\n";
    std::cout << "  Esc       return to the browse list\n";
    std::cout << "  Tab       move to the next form field\n";
    std::cout << "  Backspace delete the last typed character\n";
    std::cout << "  ?         show this help screen\n";
    std::cout << "  l         lock the vault\n";
    std::cout << "  u         unlock the vault\n";
    std::cout << "  q         quit the TUI\n";
}

void RenderListView(const ShellBrowseSnapshot& snapshot) {
    if (snapshot.entry_names.empty()) {
        if (!snapshot.filter_term.empty()) {
            std::cout
                << "No matches. Press `f` or `/` to change or clear the filter.\n";
        } else {
            std::cout << "No entries selected. Press `a` to add one.\n";
        }
        return;
    }

    if (snapshot.selected_name.empty()) {
        std::cout << "Use Up/Down or j/k to choose an entry.\n";
        return;
    }

    std::cout << "Current selection: " << snapshot.selected_name << '\n';
    std::cout
        << "Press Enter to open it, `f` or `/` to filter, `a` to add, `e` to update, or `d` to delete.\n";
}

void RenderEntryView(const ShellRuntimeState& runtime) {
    if (!runtime.session.has_value() || runtime.view_context.entry_name.empty()) {
        std::cout << "No entry details available.\n";
        return;
    }

    PasswordEntry entry =
        runtime.session->LoadEntry(runtime.view_context.entry_name);
    auto entry_guard = MakeScopedCleanse(entry);
    FrontendActionResult result = BuildShowEntryResult(std::move(entry));
    auto result_guard = MakeScopedCleanse(result);
    std::string rendered = RenderFrontendActionResult(result);
    auto rendered_guard = MakeScopedCleanse(rendered);
    std::cout << rendered << '\n';
}

void RenderReadyView(const ShellBrowseSnapshot& snapshot) {
    if (snapshot.entry_names.empty()) {
        std::cout << "Vault ready. No entries available yet.\n";
        return;
    }

    std::cout << "Vault ready. Press Esc to return to the browse list.\n";
}

void RenderLockedView() {
    std::cout << "Vault locked. Press `u` to reopen the session.\n";
}

void RenderUnlockView() {
    std::cout << "Unlock the vault with the current master password.\n";
    std::cout << "Prompt: Master password (masked)\n";
}

void RenderEntryFormField(
    std::string_view label,
    std::string_view value,
    bool focused,
    bool masked) {
    std::cout << (focused ? "> " : "  ") << label << ": ";
    if (masked) {
        std::cout << MaskSecret(value);
    } else {
        std::cout << value;
    }
    std::cout << '\n';
}

void RenderEntryFormView(const TuiEntryFormState& state) {
    if (state.mode == EntryMutationMode::kCreate) {
        std::cout << "Create a new entry.\n";
        std::cout
            << "Type values directly, use Tab to move fields, Enter to save, Esc to cancel.\n\n";
    } else {
        std::cout << "Update entry: " << state.name << '\n';
        std::cout
            << "Edit the current values, use Tab to move fields, Enter to save, Esc to cancel.\n";
        std::cout << "The entry name is fixed during updates.\n\n";
    }

    RenderEntryFormField(
        "Name",
        state.name,
        state.field == TuiEntryFormField::kName,
        false);
    RenderEntryFormField(
        "Password",
        state.password,
        state.field == TuiEntryFormField::kPassword,
        true);
    RenderEntryFormField(
        "Note",
        state.note,
        state.field == TuiEntryFormField::kNote,
        false);
}

void RenderMasterPasswordFormView(
    const TuiMasterPasswordFormState& state) {
    std::cout << "Change the master password.\n";
    std::cout
        << "Enter the new password twice, use Tab to move fields, Enter to apply, Esc to cancel.\n\n";
    RenderEntryFormField(
        "New master password",
        state.new_master_password,
        state.field == TuiMasterPasswordFormField::kNewPassword,
        true);
    RenderEntryFormField(
        "Confirm new master password",
        state.confirm_master_password,
        state.field == TuiMasterPasswordFormField::kConfirmPassword,
        true);
}

void RenderBrowseFilterView(const TuiBrowseFilterState& state) {
    std::cout << "Filter entries.\n";
    std::cout << "Type a term and press Enter to apply it.\n";
    std::cout << "Submit an empty value to clear the current filter.\n";
    std::cout << "Press Esc to cancel.\n\n";
    RenderEntryFormField("Filter", state.term, true, false);
}

void RenderExactConfirmationView(
    const TuiExactConfirmationState& state) {
    if (state.kind == FrontendCommandKind::kUpdate) {
        std::cout << "Update entry: " << state.entry_name << '\n';
        std::cout
            << "Type the entry name exactly, then press Enter to continue editing.\n";
    } else if (state.kind == FrontendCommandKind::kChangeMasterPassword) {
        std::cout << "Change the master password\n";
        std::cout << "Type CHANGE exactly, then press Enter to continue.\n";
    } else {
        std::cout << "Delete entry: " << state.entry_name << '\n';
        std::cout
            << "Type the entry name exactly, then press Enter to confirm.\n";
    }

    std::cout << "Press Esc to cancel.\n\n";
    std::cout << "> Confirmation: " << state.typed_value << '\n';
}

void RenderViewSection(
    const ShellRuntimeState& runtime,
    const ShellBrowseSnapshot& snapshot,
    const TuiRenderState& render_state) {
    std::cout << "View: " << DescribeVisibleView(runtime, render_state) << '\n';

    if (render_state.browse_filter.active) {
        RenderBrowseFilterView(render_state.browse_filter);
        return;
    }

    switch (runtime.state_machine.state()) {
        case FrontendSessionState::kShowingHelp:
            RenderHelpView();
            return;
        case FrontendSessionState::kShowingEntry:
            RenderEntryView(runtime);
            return;
        case FrontendSessionState::kLocked:
            RenderLockedView();
            return;
        case FrontendSessionState::kUnlockingSession:
            RenderUnlockView();
            return;
        case FrontendSessionState::kShowingList:
            RenderListView(snapshot);
            return;
        case FrontendSessionState::kEditingEntryForm:
            RenderEntryFormView(render_state.entry_form);
            return;
        case FrontendSessionState::kEditingMasterPasswordForm:
            RenderMasterPasswordFormView(render_state.master_password_form);
            return;
        case FrontendSessionState::kConfirmingEntryOverwrite:
        case FrontendSessionState::kConfirmingEntryDeletion:
        case FrontendSessionState::kConfirmingMasterPasswordRotation:
            RenderExactConfirmationView(render_state.exact_confirmation);
            return;
        case FrontendSessionState::kReady:
            RenderReadyView(snapshot);
            return;
        case FrontendSessionState::kRecoveringFromFailure:
            std::cout << "Recovering the last stable view...\n";
            return;
        default:
            std::cout << "Completing interactive command...\n";
            return;
    }
}

}  // namespace

void RenderScreen(
    const ShellRuntimeState& runtime,
    const TuiRenderState& render_state) {
    if (ShouldEmitTerminalControlSequences(STDOUT_FILENO)) {
        std::cout << BuildClearScreenSequence();
    }

    std::cout << "ZKVault TUI Prototype\n";
    std::cout << "Session: "
              << (ShellSessionUnlocked(runtime) ? "unlocked" : "locked")
              << " | State: " << DescribeVisibleView(runtime, render_state)
              << "\n\n";

    RenderStatusSection(render_state.status_message);
    std::cout << '\n';

    ShellBrowseSnapshot snapshot = SnapshotShellBrowseState(runtime);
    auto snapshot_guard = MakeScopedCleanse(snapshot);
    std::cout << "Browse:\n";
    RenderBrowseSnapshot(snapshot);
    std::cout << '\n';

    RenderViewSection(runtime, snapshot, render_state);

    std::cout
        << "\nKeys: Up/Down or j/k move, Enter shows, f or / filters, a adds, e updates, d deletes, m changes master password, Esc cancels/browses, ? help, l lock, u unlock, q quit.\n";
    std::cout.flush();
}

}  // namespace tui_internal
