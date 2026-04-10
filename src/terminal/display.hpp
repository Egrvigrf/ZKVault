#pragma once

#include <string>

bool ShouldEmitTerminalControlSequences(int fd);

std::string BuildClearScreenSequence();

void ClearTerminalScreenIfInteractive();
