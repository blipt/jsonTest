#include "JsonBlockPager.h"

#include "helper.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

// file structure: [ { ...json format block for formatColoredChunkBlock... }, { ...json format block for formatColoredChunkBlock... }, ... ]
// Scans the input starting from objectStart for the next root JSON object
// return offsets of the start of each JSON object in the file. Last object offset is the end of the file
// first object starts at offset > 0 '[ {' so '{' we can find somewhere after '[' character
// if objectStart == end of file, return same value, if objectStart > end of file, return -1
inline static int64_t parseJsonArray(std::ifstream& input, int64_t& objectStart)
{
    if (!input.seekg(static_cast<std::streamoff>(objectStart)))
        return -1;
    if (objectStart == input.tellg())
        return objectStart; // end of file

    throw std::runtime_error("Not implemented: parseJsonArray");
    return -1; // not reached
}
JsonBlockPager::JsonBlockPager(const std::filesystem::path& path)
{
    fileSize_ = std::filesystem::file_size(path);
    input_.open(path, std::ios::binary);
    if (!input_) throw std::runtime_error("Cannot open file: " + path.string());
}
void JsonBlockPager::calculateTotalBlocks(std::function<void(double progress)> progressCallback) noexcept
{
    if (objectOffsets_.empty())
    {
        int64_t objectStart = 0;
        while (true)
        {
            if (objectStart == fileSize_) break;
            objectStart = parseJsonArray(input_, objectStart);
            if (objectStart < 0) break;
            objectOffsets_.push_back(objectStart);
        }
    }
}
[[nodiscard]]
int64_t JsonBlockPager::totalBlocks() noexcept
{
    if(!objectOffsets_.empty()) return static_cast<int64_t>(objectOffsets_.size()) - 1;
    return 0;
}
[[nodiscard]]
std::vector<std::string> JsonBlockPager::loadBlock(int64_t index)
{
    if (index < 0 || index > totalBlocks()) return {};
    int64_t objectStart = objectOffsets_[index];
    int64_t objectEnd = objectOffsets_[index + 1];
    const int64_t length = objectEnd - objectStart;
    std::string raw(static_cast<std::size_t>(length), '\0');
    input_.clear();
    if (!input_.seekg(static_cast<std::streamoff>(objectStart))) return {};
    input_.read(raw.data(), static_cast<std::streamsize>(length));
    if (input_.gcount() != static_cast<std::streamsize>(length)) return {};
    return helper::formatColoredChunkBlock(raw);
}

