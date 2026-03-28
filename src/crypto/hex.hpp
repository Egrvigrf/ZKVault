#pragma once

#include <string>
#include <vector>

std::string BytesToHex(const std::vector<unsigned char>& bytes);
std::vector<unsigned char> HexToBytes(const std::string& hex);
