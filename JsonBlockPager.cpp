#include "JsonBlockPager.hpp"

#include "jsonWrapper.hpp"
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

JsonBlockPager::JsonBlockPager(const std::filesystem::path& path)
    : path_(path), input_(path, std::ios::binary) {
    if (!input_) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }
}

const std::filesystem::path& JsonBlockPager::path() const noexcept {
    return path_;
}

std::size_t JsonBlockPager::currentIndex() const noexcept {
    return currentIndex_;
}

bool JsonBlockPager::hasCurrent() const noexcept {
    return hasCurrent_;
}

std::size_t JsonBlockPager::totalBlocks() const noexcept {
    return objectOffsets_.size();
}

std::optional<JsonBlockPager::Block> JsonBlockPager::loadCurrent() {
    if (hasCurrent_) {
        return loadAt(currentIndex_);
    }
    return loadNext();
}

std::optional<JsonBlockPager::Block> JsonBlockPager::loadNext() {
    const std::size_t targetIndex = hasCurrent_ ? currentIndex_ + 1 : 0;
    return loadAt(targetIndex);
}

std::optional<JsonBlockPager::Block> JsonBlockPager::loadPrevious() {
    if (!hasCurrent_ || currentIndex_ == 0) {
        return std::nullopt;
    }
    return loadAt(currentIndex_ - 1);
}

std::optional<JsonBlockPager::Block> JsonBlockPager::loadAt(std::size_t index) {
    while (objectOffsets_.size() <= index && !reachedEnd_) {
        Offset objectStart = 0;
        const ScanResult scanResult = findNextObjectStart(objectStart);
        if (scanResult != ScanResult::FoundObject) {
            reachedEnd_ = true;
            return std::nullopt;
        }

        Offset afterObject = objectStart;
        readObjectRaw(objectStart, afterObject);
        objectOffsets_.push_back(objectStart);
        scanOffset_ = afterObject;
    }

    if (index >= objectOffsets_.size()) {
        return std::nullopt;
    }

    Offset afterObject = objectOffsets_[index];
    Block block{index, readObjectRaw(objectOffsets_[index], afterObject)};
    currentIndex_ = index;
    hasCurrent_ = true;
    return block;
}

JsonBlockPager::ScanResult JsonBlockPager::findNextObjectStart(Offset& objectStart) {
    seek(scanOffset_);

    bool foundArrayStart = !objectOffsets_.empty();
    if (scanOffset_ > 0) {
        foundArrayStart = true;
    }

    while (input_) {
        const char ch = readChar();
        if (!input_) {
            break;
        }

        if (isWhitespace(ch) || ch == ',') {
            continue;
        }

        if (!foundArrayStart) {
            if (ch == '[') {
                foundArrayStart = true;
                continue;
            }
            throw std::runtime_error("Expected '[' at the beginning of the JSON array");
        }

        if (ch == ']') {
            return ScanResult::EndOfArray;
        }

        if (ch == '{') {
            objectStart = tell() - 1;
            return ScanResult::FoundObject;
        }

        throw std::runtime_error("Expected object block '{' inside the JSON array");
    }

    return ScanResult::EndOfFile;
}

std::string JsonBlockPager::readObjectRaw(Offset objectStart, Offset& afterObject) {
    seek(objectStart);

    std::string raw;
    raw.reserve(4096);

    int depth = 0;
    bool insideString = false;
    bool escaped = false;

    while (input_) {
        const char ch = readChar();
        if (!input_) {
            break;
        }

        raw.push_back(ch);

        if (insideString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                insideString = false;
            }
            continue;
        }

        if (ch == '"') {
            insideString = true;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                afterObject = tell();
                return raw;
            }
        }
    }

    throw std::runtime_error("Unexpected end of file while reading object block");
}

char JsonBlockPager::readChar() {
    char ch = '\0';
    input_.get(ch);
    return ch;
}

void JsonBlockPager::seek(Offset offset) {
    input_.clear();
    input_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input_) {
        throw std::runtime_error("Cannot seek in file");
    }
}

JsonBlockPager::Offset JsonBlockPager::tell() {
    const std::streampos position = input_.tellg();
    if (position < 0) {
        throw std::runtime_error("Cannot read file position");
    }
    return static_cast<Offset>(position);
}

bool JsonBlockPager::isWhitespace(char ch) noexcept {
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

std::vector<std::string> JsonBlockPager::Block::formatColoredChunkBlock() const
{
    constexpr const char* COLOR_RESET = "\033[0m";

    auto hexToRGB = [](const std::string& hex, int& r, int& g, int& b)
    {
        std::stringstream ss;
        ss << std::hex << hex.substr(hex.starts_with('#') ? 1U : 0U);
        unsigned int hexValue = 0;
        ss >> hexValue;
        r = static_cast<int>((hexValue >> 16) & 0xFF);
        g = static_cast<int>((hexValue >> 8) & 0xFF);
        b = static_cast<int>(hexValue & 0xFF);
    };

    auto rgbToAnsi = [](int r, int g, int b)
    {
        return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    };

    auto rgbToAnsiHex = [&hexToRGB, &rgbToAnsi](const std::string& hex)
    {
        int r = 0, g = 0, b = 0;
        hexToRGB(hex, r, g, b);
        return rgbToAnsi(r, g, b);
    };

    auto toString = [](const float& val, const int& leadingSpaces, const int& precision) -> std::string
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", precision, val);
        std::string ret = std::string(buf);

        const auto pos = ret.find('.');
        if (pos == std::string::npos)
        {
            return ret;
        }

        const auto nSpaces = std::max<int>(0, leadingSpaces - static_cast<int>(pos));
        return std::string(static_cast<size_t>(nSpaces), ' ') + ret;
    };

    auto parseTimestampMs = [](const std::string& sTimestamp) -> uint64_t
    {
        uint64_t parts[4] = { 0, 0, 0, 0 };
        size_t idx = 0;
        uint64_t cur = 0;
        for (const char c : sTimestamp)
        {
            if (c >= '0' && c <= '9')
            {
                cur = cur * 10 + static_cast<uint64_t>(c - '0');
            }
            else if (c == ':' || c == '.' || c == ',')
            {
                if (idx < 4)
                {
                    parts[idx] = cur;
                }
                ++idx;
                cur = 0;
            }
        }
        if (idx < 4)
        {
            parts[idx] = cur;
        }
        return ((parts[0] * 60u + parts[1]) * 60u + parts[2]) * 1000u + parts[3];
    };

    std::vector<std::string> lines;

    const auto jConfigCurrent = nlohmann::json::parse(raw);

    if (jConfigCurrent.contains("text"))
    {
        const auto& oText = jConfigCurrent["text"];
        if (oText.is_array())
        {
            std::string sText;
            auto oTextArr = oText.get<nlohmann::json::array_t>();
            std::sort(oTextArr.begin(), oTextArr.end(), [](const nlohmann::json& a, const nlohmann::json& b)
                {
                    return a["index"] < b["index"];
                });

            for (const auto& oItem : oTextArr)
            {
                const std::string value = oItem.value("value", "");
                if (oItem.contains("color") && oItem["color"].is_string())
                {
                    sText += rgbToAnsiHex(oItem["color"].get<std::string>()) + value + COLOR_RESET;
                }
                else
                {
                    sText += value;
                }
            }
            if (!sText.empty())
            {
                lines.push_back(sText);
            }
        }
        else
        {
            lines.push_back(oText.dump());
        }

        uint64_t startMs = 0;
        if (jConfigCurrent.contains("timestamp") && jConfigCurrent["timestamp"].is_string())
        {
            startMs = parseTimestampMs(jConfigCurrent["timestamp"].get<std::string>());
        }
        if (jConfigCurrent.contains("duration") && jConfigCurrent["duration"].is_number())
        {
            const auto endMs = startMs + static_cast<uint64_t>(jConfigCurrent["duration"].get<int64_t>());
            (void)endMs;
        }
    }

    if (jConfigCurrent.contains("tokens"))
    {
        const auto& oTokens = jConfigCurrent["tokens"];
        if (oTokens.is_array())
        {
            auto oTokensArr = oTokens.get<nlohmann::json::array_t>();
            std::sort(oTokensArr.begin(), oTokensArr.end(), [](const nlohmann::json& a, const nlohmann::json& b)
                {
                    return a["index"] < b["index"];
                });
            for (const auto& oToken : oTokensArr)
            {
                std::string sTokenText;
                sTokenText += toString(oToken["p"].get<float>(), 2, 4);
                sTokenText += "|T0:";
                sTokenText += oToken["t0"].get<std::string>();
                sTokenText += "|T1:";
                sTokenText += oToken["t1"].get<std::string>();
                sTokenText += "|D:";
                sTokenText += oToken["Dur"].get<std::string>();
                sTokenText += "|OFS:";
                sTokenText += oToken["Ofs"].get<std::string>();
                sTokenText += "|DTW:";
                sTokenText += oToken["dtw"].get<std::string>();
                sTokenText += "|";

                sTokenText += rgbToAnsiHex(oToken["zone_color"].get<std::string>()) + oToken["zone"].get<std::string>() + COLOR_RESET;
                sTokenText += "|";
                sTokenText += rgbToAnsiHex(oToken["state_color"].get<std::string>()) + oToken["state"].get<std::string>() + COLOR_RESET;
                sTokenText += "|";
                sTokenText += rgbToAnsiHex(oToken["dropped_color"].get<std::string>()) + (oToken["dropped"].get<bool>() ? "D" : "U") + COLOR_RESET;
                sTokenText += "|";
                sTokenText += rgbToAnsiHex(oToken["filtered_color"].get<std::string>()) + (oToken["filtered"].get<bool>() ? "F" : "U") + COLOR_RESET;
                sTokenText += "|";
                sTokenText += rgbToAnsiHex(oToken["nonspeech_color"].get<std::string>()) + (oToken["nonspeech"].get<bool>() ? "N" : "T") + COLOR_RESET;
                sTokenText += "|";
                sTokenText += rgbToAnsiHex(oToken["color"].get<std::string>()) + oToken["value"].get<std::string>() + COLOR_RESET;
                sTokenText += "|";
                
                lines.push_back(sTokenText);
            }
        }
    }

    if (jConfigCurrent.contains("environment"))
    {
        lines.push_back(jConfigCurrent["environment"].dump());
    }
    if (jConfigCurrent.contains("error"))
    {
        lines.push_back(jConfigCurrent["error"].get<std::string>());
    }
    if (jConfigCurrent.contains("warning"))
    {
        lines.push_back(jConfigCurrent["warning"].get<std::string>());
    }
    if (jConfigCurrent.contains("info"))
    {
        lines.push_back(jConfigCurrent.dump());
    }

    if (lines.empty())
    {
        lines.push_back(raw);
    }

    return lines;
}
