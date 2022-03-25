#include "file_utils.h"

#include <fstream>

namespace FileUtils
{
    std::optional<std::vector<char>> readFile(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if(!in.is_open())
            return std::nullopt;

        std::vector<char> outData(in.tellg());
        in.seekg(0);
        in.read(outData.data(), outData.size());

        return outData;
    }
}
