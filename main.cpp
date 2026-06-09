
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#   include <Windows.h>

#   include <conio.h>

#   include <consoleapi2.h>

#   include <fcntl.h>

#else
#   include <dlfcn.h>

#   include <sys/select.h>

#   include <termios.h>

#   include <unistd.h>
#endif

#include "JsonBlockPager.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

enum class Key
{
    ArrowUp,
    ArrowDown,
    PageUp,
    PageDown,
    Quit,
    Unknown,
};

static void renderBlock(JsonBlockPager& pager, Key key, int64_t& currentIndex)
{
    try
    {
        switch (key)
        {
        case Key::ArrowUp:currentIndex--; break;
        case Key::ArrowDown:currentIndex++; break;
        case Key::PageUp:currentIndex -= 20; break;
        case Key::PageDown:currentIndex += 20; break;
        case Key::Quit: return;
        case Key::Unknown: return;
        default: return;
        }
        currentIndex = std::clamp(currentIndex, int64_t(0), static_cast<int64_t>(pager.totalBlocks()) - 1);
        std::vector<std::string> strings = std::move(pager.loadBlock(currentIndex));
        // Clear screen and move cursor to home position and print header
        std::cout << "\033[2J\033[H=== " << (currentIndex) << " / " << (pager.totalBlocks()) << " ===\n";
        for (const auto& line : strings)
            std::cout << line << '\n';
    }
    catch (const std::exception&)
    {
        std::cout << "\033[31m" << "Error rendering block" << "\033[0m" << '\n'; // Red
    }
    std::cout.flush();
}


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
        raw.c_cc[VMIN] = 1;             // блокировать до прихода 1 байта
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

static Key readKey()
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
        int64_t currentIndex = 0;
        auto dummyProgressCallback = [](int percent)
            {
                std::cout << "\rLoading... " << percent << '%' << std::flush;
            };
        pager.calculateTotalBlocks(dummyProgressCallback);
        renderBlock(pager, Key::ArrowDown, currentIndex);
        while (true)
        {
            auto key = readKey();
            if (key == Key::Quit)
                return EXIT_SUCCESS;
            if (key == Key::Unknown)
                continue;
            renderBlock(pager, key, currentIndex);
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
