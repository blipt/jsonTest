#include "JsonBlockPager.h"

#include "helper.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

// file structure: [ { ... }, { ... }, ... ]
// simplified example of file content (one line, no whitespace):
// [{ "environment": { "idle": true } },{"activity": 0,"display": true,"duration": 0,"past_tokens": 0,"process": 0,"start": "2026-06-08T01:54:35.293+0200","text": [{"value": " a"},{"value": "b"},{"value": "c"}],"timestamp": "00:00:00.000","tokens": [{"Dur": "       00.000"},{"Dur": "       00.040"},{"Dur": "       00.000"}]},{ "environment": { "run": true } }]

// Scans the input starting from current position for the next root JSON object
// Last object offset is the end of the file
// first object starts at offset > 0 '[ {' so '{' we can find somewhere after '[' character
// if objectStart == end of file, return fileSize
inline static int64_t parseJsonArray(std::ifstream& input, int64_t fileSize)
{
    char ch = '\0';

    // Phase 1: Scan forward to find the opening '{' of the next root-level object.
    // Characters between objects are '[', ']', ',', and whitespace — all skipped.
    int64_t objectStart = -1;
    while (input.get(ch))
    {
        if (ch == '{')
        {
            // tellg() is one past the character just read, so subtract 1.
            objectStart = static_cast<int64_t>(input.tellg()) - 1;
            break;
        }
    }

    if (objectStart < 0)        return fileSize; // EOF before '{': end of array
    if (objectStart > fileSize) return -1;        // Sanity: position is beyond file

    // Phase 2: Walk the object body, tracking brace depth and string state so
    // that '}' inside a string or a nested object is not mistaken for the root close.
    int  depth    = 1;
    bool inString = false;
    bool escaped  = false;

    while (depth > 0 && input.get(ch))
    {
        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (inString)
        {
            if      (ch == '\\') escaped  = true;
            else if (ch == '"')  inString = false;
            continue;
        }

        switch (ch)
        {
        case '"': inString = true; break;
        case '{': ++depth;         break;
        case '}': --depth;         break;
        }
    }

    // Stream is now positioned immediately after the closing '}', ready for
    // the next call to consume the comma / whitespace before the next object.
    return objectStart;
}
JsonBlockPager::JsonBlockPager(const std::filesystem::path& path)
{
    fileSize_ = std::filesystem::file_size(path);
    input_.open(path, std::ios::binary);
    if (!input_) throw std::runtime_error("Cannot open file: " + path.string());
}
void JsonBlockPager::calculateTotalBlocks(std::function<void(int progress)> progressCallback) noexcept
{
    if (!objectOffsets_.empty() || fileSize_ <= 0 || !input_ || !input_.seekg(0))
    {
        progressCallback(100);
        return;
    }
    int lastProgress = 0;
    progressCallback(0);
    while (true)
    {
        int64_t objectStart = parseJsonArray(input_, fileSize_);
        if (objectStart < 0) break;
        objectOffsets_.push_back(objectStart);
        const int progress = static_cast<int>(static_cast<double>(objectStart) / static_cast<double>(fileSize_) * 100);
        if (progress - lastProgress >= 5)
        {
            progressCallback(progress);
            lastProgress = progress;
        }
        if (objectStart >= fileSize_) break;
    }
}
[[nodiscard]]
int64_t JsonBlockPager::totalBlocks() noexcept
{
    if (!objectOffsets_.empty()) return static_cast<int64_t>(objectOffsets_.size()) - 1;
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
    // Drop everything after the structural closing '}'.
    const auto lastBrace = raw.rfind('}');
    if (lastBrace == std::string::npos) return {};
    raw.resize(lastBrace + 1);
    if (raw.empty()) return {};
    return helper::formatColoredChunkBlock(raw);
}