#pragma once

#include <string>
#include <vector>

#include <openssl/crypto.h>

#include "crypto/aead.hpp"
#include "model/password_entry.hpp"

inline void Cleanse(std::string& value) {
    if (!value.empty()) {
        OPENSSL_cleanse(value.data(), value.size());
    }
}

inline void Cleanse(std::vector<unsigned char>& value) {
    if (!value.empty()) {
        OPENSSL_cleanse(value.data(), value.size());
    }
}

inline void Cleanse(PasswordEntry& entry) {
    Cleanse(entry.password);
    Cleanse(entry.note);
}

inline void Cleanse(AeadCiphertext& ciphertext) {
    Cleanse(ciphertext.iv);
    Cleanse(ciphertext.ciphertext);
    Cleanse(ciphertext.auth_tag);
}

template <typename T>
class ScopedCleanse {
public:
    explicit ScopedCleanse(T& value) noexcept
        : value_(&value) {}

    ScopedCleanse(const ScopedCleanse&) = delete;
    ScopedCleanse& operator=(const ScopedCleanse&) = delete;

    ScopedCleanse(ScopedCleanse&& other) noexcept
        : value_(other.value_), active_(other.active_) {
        other.value_ = nullptr;
        other.active_ = false;
    }

    ScopedCleanse& operator=(ScopedCleanse&&) = delete;

    ~ScopedCleanse() {
        if (active_ && value_ != nullptr) {
            Cleanse(*value_);
        }
    }

    void Release() noexcept {
        active_ = false;
    }

private:
    T* value_;
    bool active_ = true;
};

template <typename T>
ScopedCleanse<T> MakeScopedCleanse(T& value) noexcept {
    return ScopedCleanse<T>(value);
}
