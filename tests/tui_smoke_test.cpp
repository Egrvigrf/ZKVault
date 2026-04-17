#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <pty.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr std::string_view kEnterAlternateScreen = "\x1b[?1049h\x1b[?25l";
constexpr std::string_view kExitAlternateScreen = "\x1b[?25h\x1b[?1049l";

class ScopedFd {
public:
    explicit ScopedFd(int fd = -1) noexcept : fd_(fd) {}

    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    ~ScopedFd() {
        Reset();
    }

    int Get() const noexcept {
        return fd_;
    }

    void Reset(int new_fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }

        fd_ = new_fd;
    }

private:
    int fd_;
};

class ScopedChildProcess {
public:
    explicit ScopedChildProcess(pid_t pid = -1) noexcept : pid_(pid) {}

    ScopedChildProcess(const ScopedChildProcess&) = delete;
    ScopedChildProcess& operator=(const ScopedChildProcess&) = delete;

    ~ScopedChildProcess() {
        if (pid_ > 0) {
            ::kill(pid_, SIGTERM);
            int status = 0;
            ::waitpid(pid_, &status, 0);
        }
    }

    pid_t Get() const noexcept {
        return pid_;
    }

    void Release() noexcept {
        pid_ = -1;
    }

private:
    pid_t pid_;
};

void Require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void WriteAll(int fd, std::string_view data) {
    std::size_t written = 0;
    while (written < data.size()) {
        const ssize_t chunk =
            ::write(fd, data.data() + written, data.size() - written);
        if (chunk <= 0) {
            throw std::runtime_error("failed to write pseudo-terminal input");
        }

        written += static_cast<std::size_t>(chunk);
    }
}

void ReadUntilContains(
    int fd,
    std::string_view needle,
    std::chrono::milliseconds timeout,
    std::string& output,
    std::size_t& cursor,
    std::string_view step_name) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::array<char, 256> buffer{};

    while (output.find(needle, cursor) == std::string::npos) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            throw std::runtime_error(
                "timed out waiting for tui output during " +
                std::string(step_name) + "; captured output: " + output);
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - now);
        pollfd read_fd{
            fd,
            POLLIN,
            0
        };
        int poll_result = 0;
        do {
            poll_result = ::poll(
                &read_fd,
                1,
                static_cast<int>(remaining.count()));
        } while (poll_result < 0 && errno == EINTR);

        if (poll_result < 0) {
            throw std::runtime_error("failed to wait for tui output");
        }

        if (poll_result == 0) {
            continue;
        }

        const ssize_t count = ::read(fd, buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EIO) {
                break;
            }

            throw std::runtime_error("failed to read tui output");
        }

        if (count == 0) {
            break;
        }

        output.append(buffer.data(), static_cast<std::size_t>(count));
    }

    cursor = output.find(needle, cursor);
    cursor += needle.size();
}

std::filesystem::path MakeTempDirectory() {
    char path_template[] = "/tmp/zkvault-tui-smoke-XXXXXX";
    char* created = ::mkdtemp(path_template);
    if (created == nullptr) {
        throw std::runtime_error("failed to create temporary directory");
    }

    return std::filesystem::path(created);
}

void TestTuiSmoke(const char* binary_path) {
    const std::filesystem::path temp_dir = MakeTempDirectory();
    const auto cleanup = [&] {
        std::error_code error;
        std::filesystem::remove_all(temp_dir, error);
    };

    {
        int master_fd = -1;
        const pid_t child_pid = ::forkpty(&master_fd, nullptr, nullptr, nullptr);
        if (child_pid < 0) {
            cleanup();
            throw std::runtime_error("failed to fork pseudo-terminal tui");
        }

        if (child_pid == 0) {
            ::setenv("TERM", "xterm-256color", 1);
            if (::chdir(temp_dir.c_str()) != 0) {
                std::perror("chdir");
                std::_Exit(1);
            }

            ::execl(binary_path, binary_path, "tui", nullptr);
            std::perror("execl");
            std::_Exit(1);
        }

        ScopedFd master(master_fd);
        ScopedChildProcess child(child_pid);

        std::string output;
        std::size_t cursor = 0;
        ReadUntilContains(
            master.Get(),
            "Vault not initialized. Create one now? [y/N]: ",
            std::chrono::seconds(2),
            output,
            cursor,
            "startup confirmation prompt");
        WriteAll(master.Get(), "y\n");

        ReadUntilContains(
            master.Get(),
            "Master password: ",
            std::chrono::seconds(2),
            output,
            cursor,
            "initial master password prompt");
        WriteAll(master.Get(), "test-master-password\n");

        ReadUntilContains(
            master.Get(),
            "Confirm master password: ",
            std::chrono::seconds(2),
            output,
            cursor,
            "master password confirmation prompt");
        WriteAll(master.Get(), "test-master-password\n");

        ReadUntilContains(
            master.Get(),
            "tui ready; use ? for help",
            std::chrono::seconds(3),
            output,
            cursor,
            "tui ready banner");
        ReadUntilContains(
            master.Get(),
            "View: list",
            std::chrono::seconds(2),
            output,
            cursor,
            "initial list view");
        Require(output.find("(empty)") != std::string::npos,
                "tui should render an empty browse state after initialization");

        Require(output.find(std::string(kEnterAlternateScreen)) != std::string::npos,
                "tui should enter the terminal alternate screen");
        Require(output.find("initialized .zkv_master") != std::string::npos,
                "tui should surface startup initialization results");
        Require(output.find("Browse:") != std::string::npos,
                "tui should render a dedicated browse section");

        WriteAll(master.Get(), "q");
        ReadUntilContains(
            master.Get(),
            kExitAlternateScreen,
            std::chrono::seconds(2),
            output,
            cursor,
            "alternate-screen teardown");

        int status = 0;
        if (::waitpid(child.Get(), &status, 0) != child.Get()) {
            cleanup();
            throw std::runtime_error("failed to wait for tui child process");
        }
        child.Release();

        Require(WIFEXITED(status), "tui child process should exit normally");
        Require(WEXITSTATUS(status) == 0, "tui child process should exit successfully");
    }

    {
        int master_fd = -1;
        const pid_t child_pid = ::forkpty(&master_fd, nullptr, nullptr, nullptr);
        if (child_pid < 0) {
            cleanup();
            throw std::runtime_error("failed to fork pseudo-terminal tui");
        }

        if (child_pid == 0) {
            ::setenv("TERM", "xterm-256color", 1);
            if (::chdir(temp_dir.c_str()) != 0) {
                std::perror("chdir");
                std::_Exit(1);
            }

            ::execl(binary_path, binary_path, "tui", nullptr);
            std::perror("execl");
            std::_Exit(1);
        }

        ScopedFd master(master_fd);
        ScopedChildProcess child(child_pid);

        std::string output;
        std::size_t cursor = 0;
        ReadUntilContains(
            master.Get(),
            "Master password: ",
            std::chrono::seconds(2),
            output,
            cursor,
            "startup unlock prompt");
        WriteAll(master.Get(), "test-master-password\n");

        ReadUntilContains(
            master.Get(),
            "tui ready; use ? for help",
            std::chrono::seconds(2),
            output,
            cursor,
            "ready banner");

        WriteAll(master.Get(), "a");
        ReadUntilContains(
            master.Get(),
            "View: edit-entry",
            std::chrono::seconds(2),
            output,
            cursor,
            "entry form view");
        ReadUntilContains(
            master.Get(),
            "Create a new entry.",
            std::chrono::seconds(2),
            output,
            cursor,
            "entry form heading");
        ReadUntilContains(
            master.Get(),
            "> Name: ",
            std::chrono::seconds(2),
            output,
            cursor,
            "entry form name field");
        WriteAll(master.Get(), "bank\tbank-password\tbank note\r");
        ReadUntilContains(
            master.Get(),
            "saved to data/bank.zkv",
            std::chrono::seconds(2),
            output,
            cursor,
            "saved bank status");
        ReadUntilContains(
            master.Get(),
            "View: list",
            std::chrono::seconds(2),
            output,
            cursor,
            "list view after saving bank");
        {
            const std::size_t list_state = output.rfind("Session: unlocked | State: list");
            Require(list_state != std::string::npos &&
                        output.find("> bank", list_state) != std::string::npos,
                    "saving bank should return to browse with bank selected");
        }

        WriteAll(master.Get(), "a");
        ReadUntilContains(
            master.Get(),
            "View: edit-entry",
            std::chrono::seconds(2),
            output,
            cursor,
            "second entry form view");
        WriteAll(master.Get(), "email\thunter2\twork login\r");
        ReadUntilContains(
            master.Get(),
            "saved to data/email.zkv",
            std::chrono::seconds(2),
            output,
            cursor,
            "saved email status");
        ReadUntilContains(
            master.Get(),
            "View: list",
            std::chrono::seconds(2),
            output,
            cursor,
            "list view");
        {
            const std::size_t list_state = output.rfind("Session: unlocked | State: list");
            Require(list_state != std::string::npos &&
                        output.find("> email", list_state) != std::string::npos,
                    "saving email should focus the newly created entry");
            Require(output.find("  bank", list_state) != std::string::npos,
                    "browse view should still render bank after adding email");
        }

        WriteAll(master.Get(), "k");
        ReadUntilContains(
            master.Get(),
            "> bank",
            std::chrono::seconds(2),
            output,
            cursor,
            "selection after moving up to bank");

        WriteAll(master.Get(), "j");
        ReadUntilContains(
            master.Get(),
            "> email",
            std::chrono::seconds(2),
            output,
            cursor,
            "selection after moving down to email");

        WriteAll(master.Get(), "\r");
        ReadUntilContains(
            master.Get(),
            "View: entry",
            std::chrono::seconds(2),
            output,
            cursor,
            "entry detail view");
        ReadUntilContains(
            master.Get(),
            "\"name\": \"email\"",
            std::chrono::seconds(2),
            output,
            cursor,
            "entry detail name");
        ReadUntilContains(
            master.Get(),
            "\"password\": \"hunter2\"",
            std::chrono::seconds(2),
            output,
            cursor,
            "entry detail password");

        WriteAll(master.Get(), "?");
        ReadUntilContains(
            master.Get(),
            "View: help",
            std::chrono::seconds(2),
            output,
            cursor,
            "help view");
        ReadUntilContains(
            master.Get(),
            "Keys:",
            std::chrono::seconds(2),
            output,
            cursor,
            "help heading");
        ReadUntilContains(
            master.Get(),
            "Enter     view the selected entry",
            std::chrono::seconds(2),
            output,
            cursor,
            "help shortcut description");
        ReadUntilContains(
            master.Get(),
            "a         create a new entry",
            std::chrono::seconds(2),
            output,
            cursor,
            "help add shortcut");
        ReadUntilContains(
            master.Get(),
            "d         delete the selected entry",
            std::chrono::seconds(2),
            output,
            cursor,
            "help delete shortcut");
        const std::size_t help_state = output.rfind("Session: unlocked | State: help");
        Require(help_state != std::string::npos &&
                    output.find("> email", help_state) != std::string::npos,
                "help view should preserve the current browse selection");

        WriteAll(master.Get(), "\x1b");
        ReadUntilContains(
            master.Get(),
            "View: list",
            std::chrono::seconds(2),
            output,
            cursor,
            "browse view after escape");
        const std::size_t list_state_after_escape =
            output.rfind("Session: unlocked | State: list");
        if (list_state_after_escape == std::string::npos ||
            output.find("> email", list_state_after_escape) ==
                std::string::npos) {
            throw std::runtime_error(
                "returning from help should preserve the current browse selection; "
                "captured output: " + output);
        }

        WriteAll(master.Get(), "d");
        ReadUntilContains(
            master.Get(),
            "View: confirm-delete",
            std::chrono::seconds(2),
            output,
            cursor,
            "delete confirmation view");
        ReadUntilContains(
            master.Get(),
            "Delete entry: email",
            std::chrono::seconds(2),
            output,
            cursor,
            "delete confirmation target");
        WriteAll(master.Get(), "\x1b");
        ReadUntilContains(
            master.Get(),
            "entry deletion cancelled",
            std::chrono::seconds(2),
            output,
            cursor,
            "delete cancellation status");
        {
            const std::size_t list_state = output.rfind("Session: unlocked | State: list");
            Require(list_state != std::string::npos &&
                        output.find("> email", list_state) != std::string::npos,
                    "cancelling delete should return to browse with email still selected");
        }

        WriteAll(master.Get(), "d");
        ReadUntilContains(
            master.Get(),
            "View: confirm-delete",
            std::chrono::seconds(2),
            output,
            cursor,
            "second delete confirmation view");
        WriteAll(master.Get(), "email\r");
        ReadUntilContains(
            master.Get(),
            "deleted data/email.zkv",
            std::chrono::seconds(2),
            output,
            cursor,
            "deleted email status");
        ReadUntilContains(
            master.Get(),
            "View: list",
            std::chrono::seconds(2),
            output,
            cursor,
            "list view after delete");
        {
            const std::size_t list_state = output.rfind("Session: unlocked | State: list");
            Require(list_state != std::string::npos &&
                        output.find("> bank", list_state) != std::string::npos,
                    "deleting email should return to browse with bank selected");
        }

        WriteAll(master.Get(), "l");
        ReadUntilContains(
            master.Get(),
            "vault locked",
            std::chrono::seconds(2),
            output,
            cursor,
            "locked status");
        ReadUntilContains(
            master.Get(),
            "View: locked",
            std::chrono::seconds(2),
            output,
            cursor,
            "locked view");

        WriteAll(master.Get(), "u");
        ReadUntilContains(
            master.Get(),
            "View: unlock",
            std::chrono::seconds(2),
            output,
            cursor,
            "unlock view");
        ReadUntilContains(
            master.Get(),
            "Prompt: Master password (masked)",
            std::chrono::seconds(2),
            output,
            cursor,
            "unlock prompt description");
        ReadUntilContains(
            master.Get(),
            "Master password: ",
            std::chrono::seconds(2),
            output,
            cursor,
            "unlock password prompt");
        WriteAll(master.Get(), "test-master-password\n");
        ReadUntilContains(
            master.Get(),
            "vault unlocked",
            std::chrono::seconds(2),
            output,
            cursor,
            "unlocked status");
        ReadUntilContains(
            master.Get(),
            "View: list",
            std::chrono::seconds(2),
            output,
            cursor,
            "list view after unlock");
        const std::size_t list_state_after_unlock =
            output.rfind("Session: unlocked | State: list");
        Require(list_state_after_unlock != std::string::npos &&
                    output.find("> bank", list_state_after_unlock) !=
                        std::string::npos,
                "unlocking should restore the browse view with a focused entry");

        WriteAll(master.Get(), "q");
        ReadUntilContains(
            master.Get(),
            kExitAlternateScreen,
            std::chrono::seconds(2),
            output,
            cursor,
            "alternate-screen teardown");

        int status = 0;
        if (::waitpid(child.Get(), &status, 0) != child.Get()) {
            cleanup();
            throw std::runtime_error("failed to wait for tui child process");
        }
        child.Release();

        Require(WIFEXITED(status), "tui child process should exit normally");
        Require(WEXITSTATUS(status) == 0, "tui child process should exit successfully");
    }

    cleanup();
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        return (std::fprintf(stderr, "usage: %s <zkvault-binary>\n", argv[0]), 1);
    }

    try {
        TestTuiSmoke(argv[1]);
        return 0;
    } catch (const std::exception& ex) {
        return (std::fprintf(stderr, "tui smoke test failed: %s\n", ex.what()), 1);
    }
}
