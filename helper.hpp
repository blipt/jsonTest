#pragma once

#include "jsonWrapper.hpp" // for nlohmann::json

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    constexpr const char* COLOR_RESET = "\033[0m";

    std::string rgbToAnsi(int r, int g, int b)
    {
        return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    }

    void hexToRGB(const std::string& hex, int& r, int& g, int& b)
    {
        std::stringstream ss;
        ss << std::hex << hex.substr(hex.starts_with('#') ? 1U : 0U);
        unsigned int hexValue = 0;
        ss >> hexValue;
        r = static_cast<int>((hexValue >> 16) & 0xFF);
        g = static_cast<int>((hexValue >> 8) & 0xFF);
        b = static_cast<int>(hexValue & 0xFF);
    }

    std::string rgbToAnsi(const std::string& hex)
    {
        int r = 0;
        int g = 0;
        int b = 0;
        hexToRGB(hex, r, g, b);
        return rgbToAnsi(r, g, b);
    }

    std::string toString(const float& val, const int& leadingSpaces, const int& precision)
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
    }

    uint64_t parseTimestampMs(const std::string& sTimestamp)
    {
        uint64_t parts[4] = { 0, 0, 0, 0 }; // hours, minutes, seconds, milliseconds
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
            parts[idx] = cur; // trailing milliseconds field
        }
        return ((parts[0] * 60u + parts[1]) * 60u + parts[2]) * 1000u + parts[3];
    }

    void appendLine(std::ostringstream& output, const std::string& line)
    {
        if (!line.empty())
        {
            output << line << '\n';
        }
    }

    std::string formatTextArray(const nlohmann::json& oText)
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
                sText += rgbToAnsi(oItem["color"].get<std::string>()) + value + COLOR_RESET;
            }
            else
            {
                sText += value;
            }
        }
        return sText;
    }

    std::string formatTokenLine(const nlohmann::json& oToken)
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

        sTokenText += rgbToAnsi(oToken["zone_color"].get<std::string>()) + oToken["zone"].get<std::string>() + COLOR_RESET;
        sTokenText += "|";
        sTokenText += rgbToAnsi(oToken["state_color"].get<std::string>()) + oToken["state"].get<std::string>() + COLOR_RESET;
        sTokenText += "|";
        sTokenText += rgbToAnsi(oToken["dropped_color"].get<std::string>()) + (oToken["dropped"].get<bool>() ? "D" : "U") + COLOR_RESET;
        sTokenText += "|";
        sTokenText += rgbToAnsi(oToken["filtered_color"].get<std::string>()) + (oToken["filtered"].get<bool>() ? "F" : "U") + COLOR_RESET;
        sTokenText += "|";
        sTokenText += rgbToAnsi(oToken["nonspeech_color"].get<std::string>()) + (oToken["nonspeech"].get<bool>() ? "N" : "T") + COLOR_RESET;
        sTokenText += "|";
        sTokenText += rgbToAnsi(oToken["color"].get<std::string>()) + oToken["value"].get<std::string>() + COLOR_RESET;
        return sTokenText;
    }

    std::string formatColoredChunkBlock(const std::string& rawBlock)
    {
        const auto jConfigCurrent = nlohmann::json::parse(rawBlock);
        std::ostringstream output;

        if (jConfigCurrent.contains("text"))
        {
            const auto& oText = jConfigCurrent["text"];
            if (oText.is_array())
            {
                appendLine(output, formatTextArray(oText));
            }
            else
            {
                appendLine(output, oText.dump());
            }

            // Keep timestamp parsing from the original chunkParser workflow so this
            // renderer can be extended later without changing how chunk timing is read.
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
                    appendLine(output, formatTokenLine(oToken));
                }
            }
        }

        if (jConfigCurrent.contains("environment"))
        {
            appendLine(output, jConfigCurrent["environment"].dump());
        }
        if (jConfigCurrent.contains("error"))
        {
            appendLine(output, jConfigCurrent["error"].get<std::string>());
        }
        if (jConfigCurrent.contains("warning"))
        {
            appendLine(output, jConfigCurrent["warning"].get<std::string>());
        }
        if (jConfigCurrent.contains("info"))
        {
            appendLine(output, jConfigCurrent.dump());
        }

        const std::string formatted = output.str();
        return formatted.empty() ? rawBlock : formatted;
    }

    void clearScreen()
    {
        std::cout << "\033[2J\033[H";
    }
} // namespace
