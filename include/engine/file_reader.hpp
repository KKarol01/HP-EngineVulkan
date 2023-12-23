#pragma once

#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include <fmt/core.h>

namespace eng {

class FileReader {
public:
    static std::vector<std::byte> read (std::filesystem::path file_path, std::ios_base::openmode open_mode) {
        std::ifstream file{file_path, open_mode | std::ios_base::in | std::ios_base::ate};
        if(!file) {
            throw std::runtime_error{fmt::format("File {} does not exist or cannot be opened.", file_path.string().c_str())};
        }

        std::vector<std::byte> data(file.tellg());
        file.seekg(0);
        file.read(reinterpret_cast<char*>(data.data()), data.size());
        file.close();
        return data;
    }
};

}