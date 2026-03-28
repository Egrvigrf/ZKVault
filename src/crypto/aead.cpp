#include "crypto/aead.hpp"

#include "crypto/random.hpp"

#include <openssl/evp.h>

#include <stdexcept>

namespace {

constexpr int kAes256KeySize = 32;
constexpr int kGcmIvSize = 12;
constexpr int kGcmTagSize = 16;

}  // namespace

AeadCiphertext EncryptAes256Gcm(
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& plaintext) {
    if (key.size() != kAes256KeySize) {
        throw std::runtime_error("AES-256-GCM requires a 32-byte key");
    }

    AeadCiphertext result;
    result.iv = GenerateRandomBytes(kGcmIvSize);
    result.ciphertext.resize(plaintext.size());
    result.auth_tag.resize(kGcmTagSize);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    }

    int out_len = 0;
    int final_len = 0;

    const bool encrypt_ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, result.iv.size(), nullptr) == 1 &&
        EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), result.iv.data()) == 1 &&
        EVP_EncryptUpdate(ctx,
                          result.ciphertext.data(),
                          &out_len,
                          plaintext.data(),
                          plaintext.size()) == 1 &&
        EVP_EncryptFinal_ex(ctx, result.ciphertext.data() + out_len, &final_len) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx,
                            EVP_CTRL_GCM_GET_TAG,
                            result.auth_tag.size(),
                            result.auth_tag.data()) == 1;

    EVP_CIPHER_CTX_free(ctx);

    if (!encrypt_ok) {
        throw std::runtime_error("AES-256-GCM encryption failed");
    }

    result.ciphertext.resize(out_len + final_len);
    return result;
}

std::vector<unsigned char> DecryptAes256Gcm(
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& auth_tag) {
    if (key.size() != kAes256KeySize) {
        throw std::runtime_error("AES-256-GCM requires a 32-byte key");
    }
    if (iv.size() != kGcmIvSize) {
        throw std::runtime_error("AES-256-GCM requires a 12-byte IV");
    }
    if (auth_tag.size() != kGcmTagSize) {
        throw std::runtime_error("AES-256-GCM requires a 16-byte tag");
    }

    std::vector<unsigned char> plaintext(ciphertext.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    }

    int out_len = 0;
    int final_len = 0;

    const bool decrypt_ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) == 1 &&
        EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) == 1 &&
        EVP_DecryptUpdate(ctx,
                          plaintext.data(),
                          &out_len,
                          ciphertext.data(),
                          ciphertext.size()) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx,
                            EVP_CTRL_GCM_SET_TAG,
                            auth_tag.size(),
                            const_cast<unsigned char*>(auth_tag.data())) == 1 &&
        EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &final_len) == 1;

    EVP_CIPHER_CTX_free(ctx);

    if (!decrypt_ok) {
        throw std::runtime_error("AES-256-GCM decryption failed");
    }

    plaintext.resize(out_len + final_len);
    return plaintext;
}
