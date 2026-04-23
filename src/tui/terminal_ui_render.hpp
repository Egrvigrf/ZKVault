#pragma once

#include "tui/terminal_ui_state.hpp"

namespace tui_internal {

void RenderScreen(
    const ShellRuntimeState& runtime,
    const TuiRenderState& render_state);

}  // namespace tui_internal
