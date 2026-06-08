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
#include <vector>

#ifdef HAS_CURSES
#include <curses.h>
#endif

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4710)
#pragma warning(disable : 4711)
#pragma warning(disable : 4865)
#pragma warning(disable : 5039)
#pragma warning(disable : 5045)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#   include <Windows.h>
#pragma warning(pop)
#   include <consoleapi2.h>
#   include <fcntl.h>

#   ifndef HAS_CURSES
#       include <conio.h>
#   endif

#else
#   include <unistd.h>
#   include <termios.h>
#   include <sys/select.h>
#   include <dlfcn.h>
#endif

#include "jsonWrapper.hpp"

#ifdef HAS_CURSES

// Global state for curses-based viewer
static std::vector<std::string> g_allLines;
static int g_scrollOffset = 0;
static int g_maxScroll = 0;
static JsonBlockPager* g_pager = nullptr;
static std::optional<JsonBlockPager::Block> g_currentBlock;

void initCurses()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Enable mouse support
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(0);
    
    // Enable scrolling
    scrollok(stdscr, TRUE);
    
    // Hide cursor
    curs_set(0);
    
    // Enable color if terminal supports it
    if (has_colors())
    {
        start_color();
        use_default_colors();
    }
}

void cleanupCurses()
{
    endwin();
}

void buildAllLines(const JsonBlockPager& /*pager*/, const JsonBlockPager::Block& block)
{
    g_allLines.clear();
    
    // Header line
    std::ostringstream header;
    header << "Object: " << (block.index + 1) << " (zero-based index " << block.index << ")";
    g_allLines.push_back(header.str());
    
    // Separator
    g_allLines.push_back("------------------------------------------------------------");
    
    // Content
    try
    {
        const auto renderedBlock = formatColoredChunkBlock(block.raw);
        std::istringstream iss(renderedBlock);
        std::string line;
        while (std::getline(iss, line))
        {
            g_allLines.push_back(line);
        }
        if (renderedBlock.empty() || renderedBlock.back() != '\n')
        {
            g_allLines.push_back("");
        }
    }
    catch (const std::exception&)
    {
        std::istringstream iss(block.raw);
        std::string line;
        while (std::getline(iss, line))
        {
            g_allLines.push_back(line);
        }
    }
    
    // Separator
    g_allLines.push_back("------------------------------------------------------------");
    
    // Footer
    g_allLines.push_back("");
    g_allLines.push_back("Navigation: Up/Down - next/prev object, PgUp/PgDn - page, Mouse wheel - scroll, q - quit");
    
    // Calculate max scroll
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)width;
    g_maxScroll = static_cast<int>(g_allLines.size()) > height 
                  ? static_cast<int>(g_allLines.size()) - height 
                  : 0;
}

void renderView()
{
    clear();
    
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)width;
    
    // Render visible lines based on scroll offset
    for (int i = 0; i < height && (g_scrollOffset + i) < static_cast<int>(g_allLines.size()); ++i)
    {
        mvaddstr(i, 0, g_allLines[g_scrollOffset + i].c_str());
    }
    
    // Show scroll position indicator
    if (g_maxScroll > 0)
    {
        int pos = g_maxScroll > 0 ? (g_scrollOffset * 100 / g_maxScroll) : 0;
        std::string status = "Scroll: " + std::to_string(pos) + "% (" + 
                            std::to_string(g_scrollOffset + 1) + "/" + 
                            std::to_string(g_maxScroll + height) + ")";
        mvaddstr(height - 1, 0, status.c_str());
    }
    
    refresh();
}

enum class CursesKey
{
    ArrowUp,
    ArrowDown,
    PageUp,
    PageDown,
    WheelUp,
    WheelDown,
    Quit,
    Unknown,
};

CursesKey readCursesKey()
{
    MEVENT mouseEvent;
    int ch = getch();
    
    if (ch == KEY_MOUSE)
    {
#ifdef PDCURSES
        if (nc_getmouse(&mouseEvent) == OK)
#else
        if (getmouse(&mouseEvent) == OK)
#endif
        {
            if (mouseEvent.bstate & BUTTON4_PRESSED)
                return CursesKey::WheelUp;
            if (mouseEvent.bstate & BUTTON5_PRESSED)
                return CursesKey::WheelDown;
        }
        return CursesKey::Unknown;
    }
    
    switch (ch)
    {
        case 'q':
        case 'Q':
        case 27: // ESC
            return CursesKey::Quit;
        case KEY_UP:
            return CursesKey::ArrowUp;
        case KEY_DOWN:
            return CursesKey::ArrowDown;
        case KEY_PPAGE:
            return CursesKey::PageUp;
        case KEY_NPAGE:
            return CursesKey::PageDown;
        default:
            return CursesKey::Unknown;
    }
}

bool handleInput(CursesKey key, int height)
{
    switch (key)
    {
        case CursesKey::ArrowUp:
            // Navigate to previous object
            if (g_pager)
            {
                auto previous = g_pager->loadPrevious();
                if (previous)
                {
                    g_currentBlock = std::move(previous);
                    buildAllLines(*g_pager, *g_currentBlock);
                    g_scrollOffset = 0;
                    return true;
                }
            }
            break;
        case CursesKey::ArrowDown:
            // Navigate to next object
            if (g_pager)
            {
                auto next = g_pager->loadNext();
                if (next)
                {
                    g_currentBlock = std::move(next);
                    buildAllLines(*g_pager, *g_currentBlock);
                    g_scrollOffset = 0;
                    return true;
                }
            }
            break;
        case CursesKey::PageUp:
            g_scrollOffset = std::max(0, g_scrollOffset - height + 2);
            return true;
        case CursesKey::PageDown:
            g_scrollOffset = std::min(g_maxScroll, g_scrollOffset + height - 2);
            return true;
        case CursesKey::WheelUp:
            g_scrollOffset = std::max(0, g_scrollOffset - 3);
            return true;
        case CursesKey::WheelDown:
            g_scrollOffset = std::min(g_maxScroll, g_scrollOffset + 3);
            return true;
        case CursesKey::Quit:
        case CursesKey::Unknown:
            break;
    }
    return false;
}

int mainCursesMode(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path-to-json-array-file>\n";
        return EXIT_FAILURE;
    }

    try
    {
        JsonBlockPager pager(argv[1]);
        g_pager = &pager;
        std::optional<JsonBlockPager::Block> block = pager.loadCurrent();
        
        initCurses();
        
        if (!block)
        {
            mvaddstr(0, 0, "No object blocks were found in the array.");
            mvaddstr(2, 0, "Press 'q' to quit.");
            refresh();
            
            while (true)
            {
                int ch = getch();
                if (ch == 'q' || ch == 'Q' || ch == 27)
                    break;
            }
            cleanupCurses();
            return EXIT_SUCCESS;
        }
        
        g_currentBlock = block;
        buildAllLines(pager, *block);
        
        bool running = true;
        while (running)
        {
            renderView();
            
            int height = 0;
            int width = 0;
            getmaxyx(stdscr, height, width);
            (void)width;
            
            CursesKey key = readCursesKey();
            
            if (key == CursesKey::Quit)
            {
                running = false;
            }
            else
            {
                handleInput(key, height);
            }
        }
        
        cleanupCurses();
        g_pager = nullptr;
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex)
    {
        cleanupCurses();
        std::cerr << "Error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}

#else // Non-curses fallback (original behavior)

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
    std::cout << "\nUse arrow keys or PageUp/PageDown to navigate, 'q' to quit.\n";
    std::cout.flush();
}

void renderMessage(const std::string& message)
{
    clearScreen();
    std::cout << message << "\n\n";
    std::cout << "Press 'q' to quit.\n";
    std::cout.flush();
}

Key readKey()
{
#ifdef _WIN32
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
#else
    return Key::Unknown;
#endif
}

int mainLegacyMode(int argc, char* argv[])
{
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
#endif // HAS_CURSES

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    SetConsoleOutputCP(65001);
#endif

#ifdef HAS_CURSES
    return mainCursesMode(argc, argv);
#else
    return mainLegacyMode(argc, argv);
#endif
}