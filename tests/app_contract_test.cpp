#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include "app/vault_app.hpp"

namespace {

class ScopedWorkspace {
public:
    ScopedWorkspace() : original_path_(std::filesystem::current_path()) {
        std::string path_template = "/tmp/zkvault-app-contract-XXXXXX";
        std::vector<char> buffer(path_template.begin(), path_template.end());
        buffer.push_back('\0');

        const char* created = ::mkdtemp(buffer.data());
        if (created == nullptr) {
            throw std::runtime_error("failed to create temporary workspace");
        }

        workspace_path_ = created;
        std::filesystem::current_path(workspace_path_);
    }

    ScopedWorkspace(const ScopedWorkspace&) = delete;
    ScopedWorkspace& operator=(const ScopedWorkspace&) = delete;

    ~ScopedWorkspace() {
        std::error_code ignored_error;
        std::filesystem::current_path(original_path_, ignored_error);
        std::filesystem::remove_all(workspace_path_, ignored_error);
    }

private:
    std::filesystem::path original_path_;
    std::filesystem::path workspace_path_;
};

void Require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <typename Fn>
void ExpectThrows(Fn&& fn, std::string_view expected_message) {
    try {
        std::forward<Fn>(fn)();
    } catch (const std::exception& ex) {
        if (std::string_view(ex.what()).find(expected_message) != std::string_view::npos) {
            return;
        }

        throw std::runtime_error(
            "unexpected exception message: " + std::string(ex.what()));
    }

    throw std::runtime_error(
        "expected exception containing: " + std::string(expected_message));
}

mode_t FileMode(const std::filesystem::path& path) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) {
        throw std::runtime_error("failed to stat file: " + path.string());
    }

    return st.st_mode & 0777;
}

void TestApplicationContract() {
    ScopedWorkspace workspace;

    const std::string initial_master_password = "master-password";
    const std::string rotated_master_password = "rotated-master-password";

    const InitializeVaultResult init_result =
        InitializeVault(InitializeVaultRequest{initial_master_password});
    Require(init_result.master_key_path == ".zkv_master",
            "init should return .zkv_master path");
    Require(std::filesystem::exists(".zkv_master"),
            ".zkv_master should be created");
    Require(FileMode(".zkv_master") == 0600,
            ".zkv_master should be written with 0600 permissions");

    ExpectThrows(
        [&] {
            InitializeVault(InitializeVaultRequest{initial_master_password});
        },
        ".zkv_master already exists");

    const StorePasswordEntryResult email_result = StorePasswordEntry(
        StorePasswordEntryRequest{
            EntryMutationMode::kCreate,
            "email",
            initial_master_password,
            "email-password",
            "initial note"
        });
    Require(email_result.entry_path == "data/email.zkv",
            "create should return entry path");
    Require(std::filesystem::exists("data/email.zkv"),
            "entry file should exist");
    Require(FileMode("data/email.zkv") == 0600,
            "entry file should be written with 0600 permissions");
    Require(!email_result.created_at.empty(),
            "created_at should be populated");
    Require(email_result.created_at == email_result.updated_at,
            "created_at and updated_at should match on create");

    StorePasswordEntry(
        StorePasswordEntryRequest{
            EntryMutationMode::kCreate,
            "bank",
            initial_master_password,
            "bank-password",
            "secondary note"
        });

    const std::vector<std::string> listed_names = ListEntryNames();
    Require(listed_names == std::vector<std::string>({"bank", "email"}),
            "list should return sorted entry names");

    ExpectThrows(
        [&] {
            StorePasswordEntry(
                StorePasswordEntryRequest{
                    EntryMutationMode::kCreate,
                    "email",
                    initial_master_password,
                    "ignored",
                    "ignored"
                });
        },
        "entry already exists");

    ExpectThrows(
        [&] {
            StorePasswordEntry(
                StorePasswordEntryRequest{
                    EntryMutationMode::kCreate,
                    "bad/name",
                    initial_master_password,
                    "ignored",
                    "ignored"
                });
        },
        "entry name may only contain letters, digits, '.', '-' and '_'");

    PasswordEntry email_entry = LoadPasswordEntry(
        LoadPasswordEntryRequest{"email", initial_master_password});
    Require(email_entry.name == "email", "loaded entry name should match");
    Require(email_entry.password == "email-password",
            "loaded entry password should match");
    Require(email_entry.note == "initial note",
            "loaded entry note should match");
    Require(email_entry.created_at == email_result.created_at,
            "loaded entry should preserve created_at");
    Require(email_entry.updated_at == email_result.updated_at,
            "loaded entry should preserve updated_at");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    const StorePasswordEntryResult updated_email_result = StorePasswordEntry(
        StorePasswordEntryRequest{
            EntryMutationMode::kUpdate,
            "email",
            initial_master_password,
            "updated-password",
            "updated note"
        });
    Require(updated_email_result.created_at == email_result.created_at,
            "update should preserve created_at");
    Require(updated_email_result.updated_at != email_result.updated_at,
            "update should refresh updated_at");

    PasswordEntry updated_email_entry = LoadPasswordEntry(
        LoadPasswordEntryRequest{"email", initial_master_password});
    Require(updated_email_entry.password == "updated-password",
            "updated entry password should match");
    Require(updated_email_entry.note == "updated note",
            "updated entry note should match");
    Require(updated_email_entry.created_at == email_result.created_at,
            "updated entry should preserve created_at");
    Require(updated_email_entry.updated_at == updated_email_result.updated_at,
            "updated entry should expose refreshed updated_at");

    ExpectThrows(
        [&] {
            StorePasswordEntry(
                StorePasswordEntryRequest{
                    EntryMutationMode::kUpdate,
                    "missing",
                    initial_master_password,
                    "ignored",
                    "ignored"
                });
        },
        "entry does not exist");

    const RotateMasterPasswordResult rotate_result = RotateMasterPassword(
        RotateMasterPasswordRequest{
            initial_master_password,
            rotated_master_password
        });
    Require(rotate_result.master_key_path == ".zkv_master",
            "rotation should return .zkv_master path");
    Require(FileMode(".zkv_master") == 0600,
            "rotation should preserve secure .zkv_master permissions");

    ExpectThrows(
        [&] {
            LoadPasswordEntry(
                LoadPasswordEntryRequest{"email", initial_master_password});
        },
        "AES-256-GCM decryption failed");

    PasswordEntry rotated_email_entry = LoadPasswordEntry(
        LoadPasswordEntryRequest{"email", rotated_master_password});
    Require(rotated_email_entry.password == "updated-password",
            "rotated password should unlock existing entries");

    const RemovePasswordEntryResult removed_bank =
        RemovePasswordEntry(RemovePasswordEntryRequest{"bank"});
    Require(removed_bank.entry_path == "data/bank.zkv",
            "remove should return deleted path");
    Require(!std::filesystem::exists("data/bank.zkv"),
            "removed entry should no longer exist");

    ExpectThrows(
        [&] {
            RemovePasswordEntry(RemovePasswordEntryRequest{"bank"});
        },
        "entry does not exist");
}

}  // namespace

int main() {
    try {
        TestApplicationContract();
        return 0;
    } catch (const std::exception& ex) {
        return (std::fprintf(stderr, "app contract test failed: %s\n", ex.what()), 1);
    }
}
