#include "JsonBlockPager.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <conio.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "jsonWrapper.hpp" // for nlohmann::json

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



    enum class Key
    {
        PageUp,
        PageDown,
        Quit,
        Unknown,
    };

    class TerminalRawMode
    {
    public:
        TerminalRawMode()
        {
#ifndef _WIN32
            if (tcgetattr(STDIN_FILENO, &original_) == 0)
            {
                enabled_ = true;
                termios raw = original_;
                raw.c_lflag &= static_cast<unsigned int>(~(ICANON | ECHO));
                raw.c_cc[VMIN] = 1;
                raw.c_cc[VTIME] = 0;
                tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            }
#endif
        }

        TerminalRawMode(const TerminalRawMode&) = delete;
        TerminalRawMode& operator=(const TerminalRawMode&) = delete;

        ~TerminalRawMode()
        {
#ifndef _WIN32
            if (enabled_)
            {
                tcsetattr(STDIN_FILENO, TCSANOW, &original_);
            }
#endif
        }

    private:
#ifndef _WIN32
        termios original_{};
        bool enabled_{ false };
#endif
    };

    void clearScreen()
    {
        std::cout << "\033[2J\033[H";
    }

    void printHelp()
    {
        std::cout << "Keys: PageDown = next object, PageUp = previous object, q/Esc = quit\n";
    }

    void renderBlock(const JsonBlockPager& pager, const JsonBlockPager::Block& block)
    {
        clearScreen();
        std::cout << "File: " << pager.path().string() << '\n';
        std::cout << "Object: " << (block.index + 1) << " (zero-based index " << block.index << ")\n";
        printHelp();
        std::cout << "------------------------------------------------------------\n";
        try
        {
            const auto renderedBlock = formatColoredChunkBlock(block.raw);
            std::cout << renderedBlock;
            if (renderedBlock.empty() || renderedBlock.back() != '\n')
            {
                std::cout << '\n';
            }
        }
        catch (const std::exception&)
        {
            std::cout << block.raw << '\n';
        }
        std::cout << "------------------------------------------------------------\n";
        std::cout.flush();
    }

    void renderMessage(const std::string& message)
    {
        clearScreen();
        std::cout << message << "\n\n";
        printHelp();
        std::cout.flush();
    }

#ifndef _WIN32
    bool readByteWithTimeout(char& ch, int timeoutMilliseconds)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(STDIN_FILENO, &readSet);

        timeval timeout{};
        timeout.tv_sec = timeoutMilliseconds / 1000;
        timeout.tv_usec = (timeoutMilliseconds % 1000) * 1000;

        const int ready = ::select(STDIN_FILENO + 1, &readSet, nullptr, nullptr, &timeout);
        return ready > 0 && ::read(STDIN_FILENO, &ch, 1) == 1;
    }
#endif

    Key readKey()
    {
#ifdef _WIN32
        const int ch = _getch();
        if (ch == 'q' || ch == 'Q' || ch == 27)
        {
            return Key::Quit;
        }
        if (ch == 0 || ch == 224)
        {
            const int extended = _getch();
            if (extended == 73)
            {
                return Key::PageUp;
            }
            if (extended == 81)
            {
                return Key::PageDown;
            }
        }
        return Key::Unknown;
#else
        char ch = '\0';
        if (::read(STDIN_FILENO, &ch, 1) != 1)
        {
            return Key::Unknown;
        }

        if (ch == 'q' || ch == 'Q')
        {
            return Key::Quit;
        }

        if (ch != '\033')
        {
            return Key::Unknown;
        }

        char introducer = '\0';
        if (!readByteWithTimeout(introducer, 50))
        {
            return Key::Quit;
        }
        if (introducer != '[')
        {
            return Key::Unknown;
        }

        std::string sequence;
        while (sequence.size() < 16)
        {
            char sequenceByte = '\0';
            if (!readByteWithTimeout(sequenceByte, 50))
            {
                return Key::Unknown;
            }

            sequence.push_back(sequenceByte);
            if (sequenceByte >= '@' && sequenceByte <= '~')
            {
                break;
            }
        }

        if (sequence == "5~")
        {
            return Key::PageUp;
        }
        if (sequence == "6~")
        {
            return Key::PageDown;
        }
        return Key::Unknown;
#endif
    }

    void printUsage(const char* executable, char* argv[], int argc)
    {
        std::cerr << "Usage: " << executable << " <path-to-json-array-file>\n";
        std::cerr << "The file must have the top-level shape: [{...},{...},...]\n";
        for (int i = 1; i < argc; ++i)
        {
            std::cerr << "  argv[" << i << "] = " << argv[i] << '\n';
        }
    }

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printUsage(argv[0], argv, argc);
        return EXIT_FAILURE;
    }

    std::cout << "Loading file: " << argv[1] << " ...\n";

    try
    {
        JsonBlockPager pager(argv[1]);
        TerminalRawMode rawMode;

        std::optional<JsonBlockPager::Block> block = pager.loadCurrent();
        if (!block)
        {
            renderMessage("No object blocks were found in the array.");
        }
        else
        {
            renderBlock(pager, *block);
        }

        while (true)
        {
            switch (readKey())
            {
            case Key::PageDown:
            {
                auto next = pager.loadNext();
                if (next)
                {
                    block = std::move(next);
                    renderBlock(pager, *block);
                }
                else
                {
                    renderMessage("End of array. Press PageUp to go back or q to quit.");
                }
                break;
            }
            case Key::PageUp:
            {
                auto previous = pager.loadPrevious();
                if (previous)
                {
                    block = std::move(previous);
                    renderBlock(pager, *block);
                }
                else
                {
                    renderMessage("Already at the first object. Press PageDown to continue or q to quit.");
                }
                break;
            }
            case Key::Quit:
                clearScreen();
                return EXIT_SUCCESS;
            case Key::Unknown:
                break;
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
