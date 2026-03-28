#pragma once

#include <cstddef>
#include <string>
#include <vector>

std::vector<unsigned char> DeriveKeyScrypt(
    const std::string& password,
    const std::vector<unsigned char>& salt,
    std::size_t key_size);
