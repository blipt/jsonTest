#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "JsonBlockPager.hpp"

#include "helper.hpp"
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

#include <Windows.h>
#include <conio.h>
#include <consoleapi2.h>
#include <fcntl.h>

#else

#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#endif

#include "jsonWrapper.hpp" // for nlohmann::json

enum class Key
{
    ArrowUp,
    ArrowDown,
    PageUp,
    PageDown,
    Quit,
    Unknown,
};

void renderBlock(const JsonBlockPager& /*pager*/, const JsonBlockPager::Block& block)
{
    clearScreen();
    //std::cout << "File: " << pager.path().string() << '\n';
    std::cout << "Object: " << (block.index + 1) << " (zero-based index " << block.index << ")\n";
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
    std::cout.flush();
}

Key readKey()
{
    const int ch = _getch();
    if (ch == 'q' || ch == 'Q' || ch == 27)
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
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path-to-json-array-file>\n";
        return EXIT_FAILURE;
    }

    try
    {
        JsonBlockPager pager(argv[1]);
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
                [[fallthrough]];
            case Key::ArrowDown:
            {
                auto next = pager.loadNext();
                if (next)
                {
                    block = std::move(next);
                    renderBlock(pager, *block);
                }
                else
                {
                    renderMessage("End of array. Press PageUp or ArrowUp to go back or q to quit.");
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
                    renderBlock(pager, *block);
                }
                else
                {
                    renderMessage("Already at the first object. Press PageDown or ArrowDown to continue or q to quit.");
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
