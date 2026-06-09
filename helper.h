#pragma once

#include <string>
#include <vector>

namespace helper
{
    std::vector<std::string> formatColoredChunkBlock(const std::string& raw);
    bool isWhitespace(char ch) noexcept;
} // namespace helper
