#pragma once

#include <chrono>
#include <optional>

#include "tui/terminal_ui_state.hpp"

namespace tui_internal {

TuiInputEvent ReadTuiInput(
    const std::optional<std::chrono::milliseconds>& idle_timeout);

}  // namespace tui_internal
