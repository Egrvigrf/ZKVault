#include "tui/terminal_ui_input.hpp"

#include <cerrno>
#include <chrono>
#include <optional>
#include <poll.h>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>

namespace tui_internal {
namespace {

class ScopedTerminalSettings {
public:
    ScopedTerminalSettings(int fd, const termios& settings) noexcept
        : fd_(fd), settings_(settings) {}

    ScopedTerminalSettings(const ScopedTerminalSettings&) = delete;
    ScopedTerminalSettings& operator=(const ScopedTerminalSettings&) = delete;

    ~ScopedTerminalSettings() {
        if (active_) {
            ::tcsetattr(fd_, TCSANOW, &settings_);
        }
    }

private:
    int fd_;
    termios settings_{};
    bool active_ = true;
};

int WaitForTerminalInput(
    const std::optional<std::chrono::milliseconds>& timeout) {
    pollfd read_fd{
        STDIN_FILENO,
        POLLIN,
        0
    };

    while (true) {
        const int result = ::poll(
            &read_fd,
            1,
            timeout.has_value() ? static_cast<int>(timeout->count()) : -1);
        if (result < 0 && errno == EINTR) {
            continue;
        }

        if (result < 0) {
            throw std::runtime_error("failed to wait for tui input");
        }

        return result;
    }
}

std::optional<unsigned char> TryReadNextByte(
    std::chrono::milliseconds timeout) {
    if (WaitForTerminalInput(timeout) == 0) {
        return std::nullopt;
    }

    while (true) {
        unsigned char ch = 0;
        const ssize_t bytes_read = ::read(STDIN_FILENO, &ch, 1);
        if (bytes_read < 0 && errno == EINTR) {
            continue;
        }

        if (bytes_read < 0) {
            throw std::runtime_error("failed to read tui input");
        }

        if (bytes_read == 0) {
            return std::nullopt;
        }

        return ch;
    }
}

TuiKey DecodeEscapeSequence() {
    const std::optional<unsigned char> second =
        TryReadNextByte(std::chrono::milliseconds(10));
    if (!second.has_value()) {
        return TuiKey::kBrowse;
    }

    if (*second != '[') {
        return TuiKey::kUnknown;
    }

    const std::optional<unsigned char> third =
        TryReadNextByte(std::chrono::milliseconds(10));
    if (!third.has_value()) {
        return TuiKey::kUnknown;
    }

    if (*third == 'A') {
        return TuiKey::kMoveUp;
    }

    if (*third == 'B') {
        return TuiKey::kMoveDown;
    }

    return TuiKey::kUnknown;
}

}  // namespace

TuiInputEvent ReadTuiInput(
    const std::optional<std::chrono::milliseconds>& idle_timeout) {
    if (::isatty(STDIN_FILENO) == 0) {
        throw std::runtime_error("tui requires interactive terminal");
    }

    termios old_settings{};
    if (::tcgetattr(STDIN_FILENO, &old_settings) != 0) {
        throw std::runtime_error("failed to read terminal settings");
    }

    termios new_settings = old_settings;
    new_settings.c_lflag &= ~(ECHO | ICANON);
#ifdef ECHOE
    new_settings.c_lflag &= ~ECHOE;
#endif
#ifdef ECHOK
    new_settings.c_lflag &= ~ECHOK;
#endif
#ifdef ECHONL
    new_settings.c_lflag &= ~ECHONL;
#endif
#ifdef ECHOCTL
    new_settings.c_lflag &= ~ECHOCTL;
#endif
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;

    if (::tcsetattr(STDIN_FILENO, TCSANOW, &new_settings) != 0) {
        throw std::runtime_error("failed to configure tui input mode");
    }

    ScopedTerminalSettings restore_settings(STDIN_FILENO, old_settings);
    if (WaitForTerminalInput(idle_timeout) == 0) {
        return {TuiInputStatus::kTimedOut, TuiKey::kUnknown};
    }

    while (true) {
        unsigned char ch = 0;
        const ssize_t bytes_read = ::read(STDIN_FILENO, &ch, 1);
        if (bytes_read < 0 && errno == EINTR) {
            continue;
        }

        if (bytes_read < 0) {
            throw std::runtime_error("failed to read tui input");
        }

        if (bytes_read == 0 || ch == 4) {
            return {TuiInputStatus::kEof, TuiKey::kUnknown};
        }

        if (ch == '\n' || ch == '\r') {
            return {TuiInputStatus::kKey, TuiKey::kShowSelection};
        }

        if (ch == '\t') {
            return {TuiInputStatus::kKey, TuiKey::kNextField};
        }

        if (ch == '\b' || ch == 127) {
            return {TuiInputStatus::kKey, TuiKey::kBackspace};
        }

        if (ch == 27) {
            return {TuiInputStatus::kKey, DecodeEscapeSequence()};
        }

        if (ch >= 32 && ch <= 126) {
            return {
                TuiInputStatus::kKey,
                TuiKey::kCharacter,
                static_cast<char>(ch)
            };
        }

        return {TuiInputStatus::kKey, TuiKey::kUnknown, '\0'};
    }
}

}  // namespace tui_internal
