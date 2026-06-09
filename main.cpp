#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#   include <Windows.h>
#   include <consoleapi2.h>
#   include <fcntl.h>
# include <conio.h>
#else
#   include <unistd.h>
#   include <termios.h>
#   include <sys/select.h>
#   include <dlfcn.h>
#endif

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
#include <vector>
#include <stdexcept>

namespace
{
    void clearScreen()
    {
        std::cout << "\033[2J\033[H";
    }
}

void renderBlock(const JsonBlockPager::Block& block, int total)
{
    clearScreen();
    std::cout << "=== Object: " << (block.index) << " / " << (total) << " ===\n";
    try
    {
        const auto lines = block.formatColoredChunkBlock();
        for (const auto& line : lines)
        {
            std::cout << line << '\n';
        }
    }
    catch (const std::exception&)
    {
        std::cout << block.raw << '\n';
    }
    std::cout.flush();
}

enum class Key
{
    ArrowUp,
    ArrowDown,
    PageUp,
    PageDown,
    Quit,
    Unknown,
};

#ifndef _WIN32
class RawTerminal
{
    termios m_saved{};
public:
    RawTerminal()
    {
        tcgetattr(STDIN_FILENO, &m_saved);
        termios raw = m_saved;
        raw.c_lflag &= ~(ICANON | ECHO); // отключить буферизацию строк и эхо
        raw.c_cc[VMIN]  = 1;             // блокировать до прихода 1 байта
        raw.c_cc[VTIME] = 0;             // без таймаута
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    ~RawTerminal()
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &m_saved);
    }
};

static int readByte()
{
    unsigned char c{};
    return (read(STDIN_FILENO, &c, 1) == 1) ? c : -1;
}
#endif

Key readKey()
{
#ifdef _WIN32
    const int ch = _getch();
    if (ch == 'q' || ch == 'Q')
        return Key::Quit;
    if (ch == 0 || ch == 224)
    {
        const int extended = _getch();
        if (extended == 73) return Key::PageUp;
        if (extended == 81) return Key::PageDown;
        if (extended == 72) return Key::ArrowUp;
        if (extended == 80) return Key::ArrowDown;
    }
    return Key::Unknown;
#else
    static RawTerminal raw;
    const int ch = readByte();
    if (ch < 0)            return Key::Unknown;
    if (ch == 'q' || ch == 'Q') return Key::Quit;
    if (ch != '\x1b')      return Key::Unknown;
    const int bracket = readByte();
    if (bracket != '[')    return Key::Unknown;
    const int code = readByte();
    if (code < 0)          return Key::Unknown;
    if (code == 'A') return Key::ArrowUp;
    if (code == 'B') return Key::ArrowDown;
    if (code == '5' || code == '6')
    {
        const int tilde = readByte();
        if (tilde == '~')
        {
            if (code == '5') return Key::PageUp;
            if (code == '6') return Key::PageDown;
        }
    }
    return Key::Unknown;
#endif
}

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    SetConsoleOutputCP(65001);
#endif
    try
    {
        if (argc != 2)
            throw std::runtime_error("Usage: " + std::string(argv[0]) + " <path-to-json-array-file>");
        JsonBlockPager pager(argv[1]);
        std::optional<JsonBlockPager::Block> block = pager.loadCurrent();
        if (!block)
            throw std::runtime_error("Error: No JSON objects found in the file.");

        const int totalBlocks = static_cast<int>(pager.totalBlocks());
        renderBlock(*block, totalBlocks);

        while (true)
        {
            switch (readKey())
            {
            case Key::PageDown:
                [[fallthrough]];
            case Key::ArrowDown:
            {
                auto next = pager.loadNext();
                if (next)
                {
                    block = std::move(next);
                    const int totalBlocks = static_cast<int>(pager.totalBlocks());
                    renderBlock(*block, totalBlocks);
                }
                break;
            }
            case Key::PageUp:
                [[fallthrough]];
            case Key::ArrowUp:
            {
                auto previous = pager.loadPrevious();
                if (previous)
                {
                    block = std::move(previous);
                    const int totalBlocks = static_cast<int>(pager.totalBlocks());
                    renderBlock(*block, totalBlocks);
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
