#pragma once

#include "model/master_key_file.hpp"

void SaveMasterKeyFile(const MasterKeyFile& file);
MasterKeyFile LoadMasterKeyFile();
