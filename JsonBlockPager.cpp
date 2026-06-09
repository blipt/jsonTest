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
    
    // Skip whitespace and find the start of the next JSON object '{'
    char ch = '\0';
    while (input.get(ch))
    {
        if (ch == '{')
        {
            // Found the start of a JSON object, record its position
            int64_t objStartPos = static_cast<int64_t>(input.tellg()) - 1; // Position of '{'
            
            int braceCount = 1;
            bool inString = false;
            bool escape = false;
            
            while (braceCount > 0 && input.get(ch))
            {
                if (escape)
                {
                    escape = false;
                    continue;
                }
                
                if (ch == '\\' && inString)
                {
                    escape = true;
                    continue;
                }
                
                if (ch == '"')
                {
                    inString = !inString;
                    continue;
                }
                
                if (!inString)
                {
                    if (ch == '{')
                        braceCount++;
                    else if (ch == '}')
                        braceCount--;
                }
            }
            
            // After closing brace, skip whitespace and check for comma or end
            while (input.get(ch))
            {
                if (ch == '}' || ch == ']')
                {
                    // End of array - store the start of this object and return end position
                    objectStart = static_cast<int64_t>(input.tellg());
                    return objStartPos;
                }
                if (ch == ',')
                {
                    // More objects follow - store the start of this object and return position after comma
                    objectStart = static_cast<int64_t>(input.tellg());
                    return objStartPos;
                }
                if (!helper::isWhitespace(ch))
                {
                    // Unexpected character
                    break;
                }
            }
            
            // If we reach here without finding ',' or '}', we're at end of file
            objectStart = static_cast<int64_t>(input.tellg());
            if (input.eof())
                objectStart = static_cast<int64_t>(fileSize_);
            return objStartPos;
        }
        else if (ch == ']' || ch == '}')
        {
            // End of array reached
            objectStart = static_cast<int64_t>(input.tellg());
            if (input.eof())
                objectStart = static_cast<int64_t>(fileSize_);
            return objectStart;
        }
        // Skip other characters (whitespace, '[', etc.)
    }
    
    // End of file reached
    objectStart = static_cast<int64_t>(fileSize_);
    return objectStart;
}
JsonBlockPager::JsonBlockPager(const std::filesystem::path& path)
{
    fileSize_ = std::filesystem::file_size(path);
    input_.open(path, std::ios::binary);
    if (!input_) throw std::runtime_error("Cannot open file: " + path.string());
}
void JsonBlockPager::calculateTotalBlocks(std::function<void(double progress)> progressCallback) noexcept
{
    (void)progressCallback; // unused for now
    if (objectOffsets_.empty())
    {
        int64_t objectStart = 0;
        while (true)
        {
            if (objectStart >= fileSize_) break;
            int64_t nextStart = parseJsonArray(input_, objectStart);
            if (nextStart < 0) break;
            objectOffsets_.push_back(nextStart);
            objectStart = nextStart;
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


