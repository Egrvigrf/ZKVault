#include "tui/terminal_ui_flow.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <termios.h>
#include <utility>
#include <unistd.h>

#include "crypto/secure_memory.hpp"
#include "shell/shell_runtime.hpp"
#include "terminal/display.hpp"
#include "tui/terminal_ui_input.hpp"
#include "tui/terminal_ui_render.hpp"
#include "tui/terminal_ui_state.hpp"

namespace tui_internal {
namespace {

class ScopedAlternateScreen {
public:
    ScopedAlternateScreen() {
        active_ = ShouldEmitTerminalControlSequences(STDOUT_FILENO);
        if (!active_) {
            return;
        }

        std::cout << BuildEnterAlternateScreenSequence()
                  << BuildClearScreenSequence();
        std::cout.flush();
    }

    ScopedAlternateScreen(const ScopedAlternateScreen&) = delete;
    ScopedAlternateScreen& operator=(const ScopedAlternateScreen&) = delete;

    ~ScopedAlternateScreen() {
        if (!active_) {
            return;
        }

        std::cout << BuildExitAlternateScreenSequence();
        std::cout.flush();
    }

private:
    bool active_ = false;
};

void ActivateBrowseView(ShellRuntimeState& runtime) {
    if (!runtime.session.has_value()) {
        return;
    }

    FrontendActionResult result = ShowCurrentShellBrowseView(runtime);
    auto result_guard = MakeScopedCleanse(result);
}

std::optional<FrontendCommand> ResolveTuiCommand(
    const ShellRuntimeState& runtime,
    const TuiInputEvent& input_event) {
    switch (input_event.key) {
        case TuiKey::kMoveUp:
            if (!runtime.session.has_value()) {
                return std::nullopt;
            }
            return FrontendCommand{FrontendCommandKind::kPrev, ""};
        case TuiKey::kMoveDown:
            if (!runtime.session.has_value()) {
                return std::nullopt;
            }
            return FrontendCommand{FrontendCommandKind::kNext, ""};
        case TuiKey::kShowSelection:
            if (!runtime.session.has_value()) {
                return std::nullopt;
            }
            return FrontendCommand{FrontendCommandKind::kShow, ""};
        case TuiKey::kBrowse:
            if (!runtime.session.has_value()) {
                return std::nullopt;
            }
            return FrontendCommand{FrontendCommandKind::kList, ""};
        case TuiKey::kCharacter: {
            const char ch = input_event.text;
            if (ch == '?') {
                return FrontendCommand{FrontendCommandKind::kHelp, ""};
            }

            const unsigned char value = static_cast<unsigned char>(ch);
            const char lower =
                value >= 'A' && value <= 'Z'
                    ? static_cast<char>(value - 'A' + 'a')
                    : static_cast<char>(value);
            switch (lower) {
                case 'j':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kNext, ""};
                case 'k':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kPrev, ""};
                case 'a':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kAdd, ""};
                case 'f':
                case '/':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kFind, ""};
                case 'e':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kUpdate, ""};
                case 'd':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kDelete, ""};
                case 'm':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{
                        FrontendCommandKind::kChangeMasterPassword,
                        ""
                    };
                case 'l':
                    return FrontendCommand{FrontendCommandKind::kLock, ""};
                case 'u':
                    return FrontendCommand{FrontendCommandKind::kUnlock, ""};
                case 'q':
                    return FrontendCommand{FrontendCommandKind::kQuit, ""};
                default:
                    return std::nullopt;
            }
        }
        default:
            return std::nullopt;
    }
}

void ReplaceStatusWithError(
    TuiRenderState& render_state,
    std::string_view message) {
    FrontendError error = ClassifyFrontendError(message);
    std::string output = RenderFrontendError(error);
    auto error_guard = MakeScopedCleanse(error);
    auto output_guard = MakeScopedCleanse(output);
    ReplaceStatusMessage(render_state, std::move(output));
}

void RestoreBrowseView(ShellRuntimeState& runtime) {
    FrontendActionResult result = ShowCurrentShellBrowseView(runtime);
    auto result_guard = MakeScopedCleanse(result);
}

void BeginAddEntryFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    ClearPendingCommand(render_state);
    ClearMasterPasswordForm(render_state);
    ClearBrowseFilterForm(render_state);
    ClearExactConfirmation(render_state);
    BeginEntryForm(render_state, EntryMutationMode::kCreate, "");
    ReplaceStatusMessage(render_state, "creating entry");
    static_cast<void>(runtime.state_machine.HandleCommand(FrontendCommandKind::kAdd));
}

void BeginUpdateEntryFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    const std::optional<std::string> selected_name =
        SelectedBrowseEntryName(runtime);
    if (!selected_name.has_value()) {
        ReplaceStatusWithError(render_state, "no entry selected");
        return;
    }

    ClearPendingCommand(render_state);
    ClearEntryForm(render_state);
    ClearMasterPasswordForm(render_state);
    ClearBrowseFilterForm(render_state);
    BeginExactConfirmation(
        render_state,
        FrontendCommandKind::kUpdate,
        *selected_name,
        BuildOverwriteConfirmationRule(*selected_name));
    ReplaceStatusMessage(
        render_state,
        "type the selected entry name to confirm update");
    static_cast<void>(runtime.state_machine.HandleCommand(FrontendCommandKind::kUpdate));
}

void BeginDeleteEntryFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    const std::optional<std::string> selected_name =
        SelectedBrowseEntryName(runtime);
    if (!selected_name.has_value()) {
        ReplaceStatusWithError(render_state, "no entry selected");
        return;
    }

    ClearPendingCommand(render_state);
    ClearEntryForm(render_state);
    ClearMasterPasswordForm(render_state);
    ClearBrowseFilterForm(render_state);
    BeginExactConfirmation(
        render_state,
        FrontendCommandKind::kDelete,
        *selected_name,
        BuildDeletionConfirmationRule(*selected_name));
    ReplaceStatusMessage(
        render_state,
        "type the selected entry name to confirm deletion");
    static_cast<void>(runtime.state_machine.HandleCommand(FrontendCommandKind::kDelete));
}

void BeginMasterPasswordRotationFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    ClearPendingCommand(render_state);
    ClearEntryForm(render_state);
    ClearMasterPasswordForm(render_state);
    ClearBrowseFilterForm(render_state);
    BeginExactConfirmation(
        render_state,
        FrontendCommandKind::kChangeMasterPassword,
        "",
        BuildMasterPasswordRotationConfirmationRule());
    ReplaceStatusMessage(
        render_state,
        "type CHANGE to confirm master password rotation");
    static_cast<void>(
        runtime.state_machine.HandleCommand(
            FrontendCommandKind::kChangeMasterPassword));
}

void BeginBrowseFilterFlow(TuiRenderState& render_state) {
    ClearPendingCommand(render_state);
    ClearEntryForm(render_state);
    ClearMasterPasswordForm(render_state);
    ClearExactConfirmation(render_state);
    BeginBrowseFilterForm(render_state);
    ReplaceStatusMessage(
        render_state,
        "type a filter term; submit an empty value to clear the current filter");
}

void CancelEntryForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    const std::string status_message =
        render_state.entry_form.mode == EntryMutationMode::kCreate
            ? "entry creation cancelled"
            : "entry update cancelled";
    ClearEntryForm(render_state);
    RestoreBrowseView(runtime);
    ReplaceStatusMessage(render_state, status_message);
}

void CancelMasterPasswordForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    ClearMasterPasswordForm(render_state);
    RestoreBrowseView(runtime);
    ReplaceStatusMessage(render_state, "master password rotation cancelled");
}

void CancelBrowseFilterForm(TuiRenderState& render_state) {
    ClearBrowseFilterForm(render_state);
    ReplaceStatusMessage(render_state, "browse filter cancelled");
}

void CancelExactConfirmation(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    std::string status_message) {
    ClearExactConfirmation(render_state);
    RestoreBrowseView(runtime);
    ReplaceStatusMessage(render_state, std::move(status_message));
}

void SubmitEntryForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    try {
        FrontendActionResult result = StoreShellEntryWithContent(
            runtime,
            render_state.entry_form.mode,
            render_state.entry_form.name,
            render_state.entry_form.password,
            render_state.entry_form.note);
        auto result_guard = MakeScopedCleanse(result);
        const std::string status_message = RenderTuiStatusMessage(result);
        ClearEntryForm(render_state);
        RestoreBrowseView(runtime);
        if (status_message.empty()) {
            ClearStatusMessage(render_state);
        } else {
            ReplaceStatusMessage(render_state, status_message);
        }
    } catch (const std::exception& ex) {
        ReplaceStatusWithError(render_state, ex.what());
    }
}

void SubmitMasterPasswordForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    if (render_state.master_password_form.new_master_password !=
        render_state.master_password_form.confirm_master_password) {
        ::Cleanse(render_state.master_password_form.confirm_master_password);
        render_state.master_password_form.confirm_master_password.clear();
        render_state.master_password_form.field =
            TuiMasterPasswordFormField::kConfirmPassword;
        ReplaceStatusWithError(
            render_state,
            "new master passwords do not match");
        return;
    }

    try {
        FrontendActionResult result = RotateShellMasterPassword(
            runtime,
            render_state.master_password_form.new_master_password);
        auto result_guard = MakeScopedCleanse(result);
        const std::string status_message = RenderTuiStatusMessage(result);
        ClearMasterPasswordForm(render_state);
        RestoreBrowseView(runtime);
        if (status_message.empty()) {
            ClearStatusMessage(render_state);
        } else {
            ReplaceStatusMessage(render_state, status_message);
        }
    } catch (const std::exception& ex) {
        ReplaceStatusWithError(render_state, ex.what());
    }
}

void SubmitBrowseFilterForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    try {
        FrontendActionResult result = ExecuteShellCommand(
            runtime,
            FrontendCommand{
                FrontendCommandKind::kFind,
                render_state.browse_filter.term
            });
        auto result_guard = MakeScopedCleanse(result);
        const std::string status_message = RenderTuiStatusMessage(result);
        ClearBrowseFilterForm(render_state);
        if (status_message.empty()) {
            ClearStatusMessage(render_state);
        } else {
            ReplaceStatusMessage(render_state, status_message);
        }
    } catch (const std::exception& ex) {
        ReplaceStatusWithError(render_state, ex.what());
    }
}

bool HandleEntryFormInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event) {
    if (!render_state.entry_form.active) {
        return false;
    }

    if (input_event.key == TuiKey::kBrowse) {
        CancelEntryForm(runtime, render_state);
        return true;
    }

    if (input_event.key == TuiKey::kBackspace) {
        EraseLastCharacter(ActiveEntryFormFieldValue(render_state.entry_form));
        return true;
    }

    if (input_event.text != '\0') {
        ActiveEntryFormFieldValue(render_state.entry_form).push_back(input_event.text);
        return true;
    }

    if (input_event.key == TuiKey::kNextField) {
        AdvanceEntryFormField(render_state.entry_form);
        return true;
    }

    if (input_event.key != TuiKey::kShowSelection) {
        return true;
    }

    if (render_state.entry_form.field != TuiEntryFormField::kNote) {
        AdvanceEntryFormField(render_state.entry_form);
        return true;
    }

    SubmitEntryForm(runtime, render_state);
    return true;
}

bool HandleBrowseFilterInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event) {
    if (!render_state.browse_filter.active) {
        return false;
    }

    if (input_event.key == TuiKey::kBrowse) {
        CancelBrowseFilterForm(render_state);
        return true;
    }

    if (input_event.key == TuiKey::kBackspace) {
        EraseLastCharacter(
            ActiveBrowseFilterFieldValue(render_state.browse_filter));
        return true;
    }

    if (input_event.text != '\0') {
        ActiveBrowseFilterFieldValue(render_state.browse_filter).push_back(
            input_event.text);
        return true;
    }

    if (input_event.key == TuiKey::kShowSelection) {
        SubmitBrowseFilterForm(runtime, render_state);
        return true;
    }

    return true;
}

bool HandleMasterPasswordFormInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event) {
    if (!render_state.master_password_form.active) {
        return false;
    }

    if (input_event.key == TuiKey::kBrowse) {
        CancelMasterPasswordForm(runtime, render_state);
        return true;
    }

    if (input_event.key == TuiKey::kBackspace) {
        EraseLastCharacter(
            ActiveMasterPasswordFormFieldValue(
                render_state.master_password_form));
        return true;
    }

    if (input_event.text != '\0') {
        ActiveMasterPasswordFormFieldValue(
            render_state.master_password_form).push_back(input_event.text);
        return true;
    }

    if (input_event.key == TuiKey::kNextField) {
        AdvanceMasterPasswordFormField(render_state.master_password_form);
        return true;
    }

    if (input_event.key != TuiKey::kShowSelection) {
        return true;
    }

    if (render_state.master_password_form.field !=
        TuiMasterPasswordFormField::kConfirmPassword) {
        AdvanceMasterPasswordFormField(render_state.master_password_form);
        return true;
    }

    SubmitMasterPasswordForm(runtime, render_state);
    return true;
}

void SubmitExactConfirmation(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    if (render_state.exact_confirmation.typed_value !=
        render_state.exact_confirmation.rule.expected_value) {
        CancelExactConfirmation(
            runtime,
            render_state,
            RenderFrontendError(FrontendError{
                FrontendErrorKind::kConfirmationRejected,
                render_state.exact_confirmation.rule.mismatch_error
            }));
        return;
    }

    try {
        if (render_state.exact_confirmation.kind ==
            FrontendCommandKind::kChangeMasterPassword) {
            static_cast<void>(runtime.state_machine.HandleConfirmationAccepted());
            ClearExactConfirmation(render_state);
            BeginMasterPasswordForm(render_state);
            ReplaceStatusMessage(
                render_state,
                "enter the new master password");
            return;
        }

        if (render_state.exact_confirmation.kind == FrontendCommandKind::kUpdate) {
            if (!runtime.session.has_value()) {
                throw std::runtime_error("vault is locked");
            }

            PasswordEntry entry =
                runtime.session->LoadEntry(render_state.exact_confirmation.entry_name);
            auto entry_guard = MakeScopedCleanse(entry);
            static_cast<void>(runtime.state_machine.HandleConfirmationAccepted());
            ClearExactConfirmation(render_state);
            PopulateEntryFormForUpdate(render_state, entry);
            ReplaceStatusMessage(
                render_state,
                "editing entry " + render_state.entry_form.name);
            return;
        }

        static_cast<void>(runtime.state_machine.HandleConfirmationAccepted());
        FrontendActionResult result = RemoveShellEntryByName(
            runtime,
            render_state.exact_confirmation.entry_name);
        auto result_guard = MakeScopedCleanse(result);
        const std::string status_message = RenderTuiStatusMessage(result);
        ClearExactConfirmation(render_state);
        RestoreBrowseView(runtime);
        if (status_message.empty()) {
            ClearStatusMessage(render_state);
        } else {
            ReplaceStatusMessage(render_state, status_message);
        }
    } catch (const std::exception& ex) {
        ClearExactConfirmation(render_state);
        RestoreBrowseView(runtime);
        ReplaceStatusWithError(render_state, ex.what());
    }
}

bool HandleExactConfirmationInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event) {
    if (!render_state.exact_confirmation.active) {
        return false;
    }

    if (input_event.key == TuiKey::kBrowse) {
        CancelExactConfirmation(
            runtime,
            render_state,
            render_state.exact_confirmation.kind ==
                    FrontendCommandKind::kUpdate
                ? "entry update cancelled"
                : (render_state.exact_confirmation.kind ==
                           FrontendCommandKind::kChangeMasterPassword
                       ? "master password rotation cancelled"
                       : "entry deletion cancelled"));
        return true;
    }

    if (input_event.key == TuiKey::kBackspace) {
        EraseLastCharacter(render_state.exact_confirmation.typed_value);
        return true;
    }

    if (input_event.text != '\0') {
        render_state.exact_confirmation.typed_value.push_back(input_event.text);
        return true;
    }

    if (input_event.key == TuiKey::kShowSelection) {
        SubmitExactConfirmation(runtime, render_state);
        return true;
    }

    return true;
}

}  // namespace

int RunTerminalUiLoop() {
    ScopedAlternateScreen alternate_screen;

    OpenShellRuntimeResult open_result = OpenOrInitializeShellRuntime();
    auto open_result_guard = MakeScopedCleanse(open_result);
    ShellRuntimeState& runtime = open_result.runtime;
    const std::optional<std::chrono::milliseconds> idle_timeout =
        ReadShellIdleTimeout();

    FrontendActionResult ready_result = BuildTuiReadyResult();
    auto ready_result_guard = MakeScopedCleanse(ready_result);
    static_cast<void>(runtime.state_machine.ApplyActionResult(ready_result));
    ActivateBrowseView(runtime);

    TuiRenderState render_state;
    auto render_state_guard = MakeScopedCleanse(render_state);

    std::string initial_status;
    if (open_result.startup_result.has_value()) {
        initial_status = RenderTuiStatusMessage(*open_result.startup_result);
    }

    const std::string ready_status = RenderTuiStatusMessage(ready_result);
    if (!initial_status.empty() && !ready_status.empty()) {
        initial_status += '\n';
    }
    initial_status += ready_status;
    ReplaceStatusMessage(render_state, std::move(initial_status));

    while (true) {
        RenderScreen(runtime, render_state);

        const TuiInputEvent input_event = ReadTuiInput(
            ShellSessionUnlocked(runtime) ? idle_timeout : std::nullopt);
        if (input_event.status == TuiInputStatus::kTimedOut) {
            ClearPendingCommand(render_state);
            ClearEntryForm(render_state);
            ClearMasterPasswordForm(render_state);
            ClearBrowseFilterForm(render_state);
            ClearExactConfirmation(render_state);
            FrontendActionResult result = HandleShellIdleTimeout(runtime);
            auto result_guard = MakeScopedCleanse(result);
            ReplaceStatusMessage(
                render_state,
                RenderTuiStatusMessage(result));
            continue;
        }

        if (input_event.status == TuiInputStatus::kEof) {
            std::cout << '\n';
            return 0;
        }

        if (HandleEntryFormInput(runtime, render_state, input_event)) {
            continue;
        }

        if (HandleBrowseFilterInput(runtime, render_state, input_event)) {
            continue;
        }

        if (HandleMasterPasswordFormInput(runtime, render_state, input_event)) {
            continue;
        }

        if (HandleExactConfirmationInput(runtime, render_state, input_event)) {
            continue;
        }

        const std::optional<FrontendCommand> command =
            ResolveTuiCommand(runtime, input_event);
        if (!command.has_value()) {
            continue;
        }

        try {
            if (command->kind == FrontendCommandKind::kAdd) {
                BeginAddEntryFlow(runtime, render_state);
                continue;
            }

            if (command->kind == FrontendCommandKind::kUpdate) {
                BeginUpdateEntryFlow(runtime, render_state);
                continue;
            }

            if (command->kind == FrontendCommandKind::kDelete) {
                BeginDeleteEntryFlow(runtime, render_state);
                continue;
            }

            if (command->kind == FrontendCommandKind::kFind) {
                BeginBrowseFilterFlow(render_state);
                continue;
            }

            if (command->kind == FrontendCommandKind::kChangeMasterPassword) {
                BeginMasterPasswordRotationFlow(runtime, render_state);
                continue;
            }

            FrontendActionResult result{};
            if (command->kind == FrontendCommandKind::kList) {
                result = ShowCurrentShellBrowseView(runtime);
            } else if (ShouldPreviewPreparedCommand(runtime, *command)) {
                ReplacePendingCommand(render_state, *command);
                ReplaceStatusMessage(
                    render_state,
                    BuildPendingStatusMessage(*command));
                static_cast<void>(runtime.state_machine.HandleCommand(command->kind));
                RenderScreen(runtime, render_state);
                result = ExecutePreparedShellCommand(runtime, *command);
                if (command->kind == FrontendCommandKind::kUnlock &&
                    runtime.session.has_value()) {
                    ActivateBrowseView(runtime);
                }
                ClearPendingCommand(render_state);
            } else {
                result = ExecuteShellCommand(runtime, *command);
            }
            auto result_guard = MakeScopedCleanse(result);
            const std::string status_message = RenderTuiStatusMessage(result);
            if (status_message.empty()) {
                ClearStatusMessage(render_state);
            } else {
                ReplaceStatusMessage(render_state, status_message);
            }
            if (runtime.state_machine.state() ==
                FrontendSessionState::kQuitRequested) {
                return 0;
            }
        } catch (const std::exception& ex) {
            FrontendError error = ClassifyFrontendError(ex.what());
            std::string output = RenderFrontendError(error);
            auto error_guard = MakeScopedCleanse(error);
            auto output_guard = MakeScopedCleanse(output);
            ClearPendingCommand(render_state);
            ClearEntryForm(render_state);
            ClearMasterPasswordForm(render_state);
            ClearBrowseFilterForm(render_state);
            ClearExactConfirmation(render_state);
            static_cast<void>(RecoverShellViewAfterFailure(runtime));
            ReplaceStatusMessage(render_state, std::move(output));
        }
    }
}

}  // namespace tui_internal
