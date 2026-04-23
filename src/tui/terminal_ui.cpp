#include "tui/terminal_ui.hpp"

#include "tui/terminal_ui_flow.hpp"

int RunTerminalUi() {
    return tui_internal::RunTerminalUiLoop();
}
