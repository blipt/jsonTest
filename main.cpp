
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#   include <Windows.h>
#   include <conio.h>
#   include <consoleapi2.h>
#   include <fcntl.h>
#else
#   include <dlfcn.h>
#   include <sys/ioctl.h>
#   include <sys/select.h>
#   include <termios.h>
#   include <unistd.h>
#endif

#include "JsonBlockPager.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class LinePager
{
public:
    static constexpr int CACHE_CAPACITY = 64;
    explicit LinePager(JsonBlockPager& pager) : m_pager(pager)
    {
        if (pager.totalBlocks() > 0)ensureCached(0);
    }
    void scroll(int64_t delta)
    {
        if (delta > 0) moveForward(delta);
        else if (delta < 0) moveBackward(-delta);
    }
    std::vector<std::string> visibleLines(int rows)
    {
        std::vector<std::string> out;
        out.reserve(rows);
        int64_t blk = m_blockIdx;
        int64_t ln  = m_lineInBlock;
        while (static_cast<int>(out.size()) < rows && blk < m_pager.totalBlocks())
        {
            ensureCached(blk);
            for (const auto& cb : m_cache)
            {
                if (cb.idx != blk) continue;
                while (ln < static_cast<int64_t>(cb.lines.size()) && static_cast<int>(out.size()) < rows)
                    out.push_back(cb.lines[ln++]);
                break;
            }
            ++blk;
            ln = 0;
        }
        return out;
    }
    int64_t totalBlocks()      const { return m_pager.totalBlocks(); }
    int64_t currentBlock()     const { return m_blockIdx; }
    int64_t currentLineInBlock() const { return m_lineInBlock; }
private:
    struct CachedBlock
    {
        int64_t                  idx = -1;
        std::vector<std::string> lines;
    };
    JsonBlockPager&         m_pager;
    std::deque<CachedBlock> m_cache;
    int64_t                 m_blockIdx    = 0;
    int64_t                 m_lineInBlock = 0;
    bool isCached(int64_t idx) const
    {
        for (const auto& cb : m_cache)
            if (cb.idx == idx) return true;
        return false;
    }
    void ensureCached(int64_t idx)
    {
        if (idx < 0 || idx >= m_pager.totalBlocks()) return;
        if (isCached(idx)) return;
        if (static_cast<int>(m_cache.size()) >= CACHE_CAPACITY)
        {
            auto victim  = m_cache.begin();
            int64_t maxD = 0;
            for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
            {
                int64_t d = it->idx >= m_blockIdx ? it->idx - m_blockIdx : m_blockIdx - it->idx;
                if (d > maxD) { maxD = d; victim = it; }
            }
            m_cache.erase(victim);
        }
        m_cache.push_back({idx, m_pager.loadBlock(idx)});
    }
    int64_t blockLineCount(int64_t idx)
    {
        ensureCached(idx);
        for (const auto& cb : m_cache)
            if (cb.idx == idx) return static_cast<int64_t>(cb.lines.size());
        return 0;
    }
    void moveForward(int64_t delta)
    {
        while (delta > 0)
        {
            const int64_t N = blockLineCount(m_blockIdx);
            if (N == 0) return;
            const int64_t available = N - 1 - m_lineInBlock;
            if (delta <= available)
            {
                m_lineInBlock += delta;
                return;
            }
            if (m_blockIdx + 1 >= m_pager.totalBlocks())
            {
                m_lineInBlock = N - 1;
                return;
            }
            delta -= available + 1;
            ++m_blockIdx;
            m_lineInBlock = 0;
        }
    }
    void moveBackward(int64_t delta)
    {
        while (delta > 0)
        {
            if (delta <= m_lineInBlock)
            {
                m_lineInBlock -= delta;
                return;
            }
            if (m_blockIdx == 0)
            {
                m_lineInBlock = 0;
                return;
            }
            delta -= m_lineInBlock + 1;
            --m_blockIdx;
            const int64_t N = blockLineCount(m_blockIdx);
            m_lineInBlock = (N > 0) ? N - 1 : 0;
        }
    }
};
static int terminalRows()
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return 24;
#else
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 1)
        return static_cast<int>(ws.ws_row);
    return 24;
#endif
}
enum class Key { ArrowUp, ArrowDown, PageUp, PageDown, Quit, Unknown };
#ifndef _WIN32
class RawTerminal
{
    termios m_saved{};
public:
    RawTerminal()
    {
        tcgetattr(STDIN_FILENO, &m_saved);
        termios raw = m_saved;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    ~RawTerminal() { tcsetattr(STDIN_FILENO, TCSANOW, &m_saved); }
};

static int readByte()
{
    unsigned char c{};
    return (read(STDIN_FILENO, &c, 1) == 1) ? static_cast<int>(c) : -1;
}
#endif

static Key readKey()
{
#ifdef _WIN32
    const int ch = _getch();
    if (ch == 'q' || ch == 'Q') return Key::Quit;
    if (ch == 0 || ch == 224)
    {
        const int ext = _getch();
        if (ext == 73) return Key::PageUp;
        if (ext == 81) return Key::PageDown;
        if (ext == 72) return Key::ArrowUp;
        if (ext == 80) return Key::ArrowDown;
    }
    return Key::Unknown;
#else
    static RawTerminal raw;
    const int ch = readByte();
    if (ch < 0)                  return Key::Unknown;
    if (ch == 'q' || ch == 'Q') return Key::Quit;
    if (ch != '\x1b')            return Key::Unknown;
    if (readByte() != '[')       return Key::Unknown;
    const int code = readByte();
    if (code < 0)                return Key::Unknown;
    if (code == 'A') return Key::ArrowUp;
    if (code == 'B') return Key::ArrowDown;
    if (code == '5' || code == '6')
    {
        if (readByte() == '~')
        {
            if (code == '5') return Key::PageUp;
            if (code == '6') return Key::PageDown;
        }
    }
    return Key::Unknown;
#endif
}
static void render(LinePager& pager)
{
    const int rows = terminalRows();
    const int contentRows = rows - 1;
    std::vector<std::string> lines;
    try
    {
        lines = pager.visibleLines(contentRows);
    }
    catch (const std::exception& e)
    {
        std::cout << "\033[31mError: " << e.what() << "\033[0m\n" << std::flush;
        return;
    }
    // Clear screen, history and move cursor to top-left corner
    std::cout << "\033[2J\033[3J\033[H";
    std::cout << " Block " << (pager.currentBlock() + 1) << "/" << pager.totalBlocks() << "\n";
    int size = static_cast<int>(lines.size());
    for (int i = 0; i < size; ++i)
    {
        std::cout << lines[i];
        if (i < size - 1)
            std::cout << "\n";
    }
    std::cout << std::flush;
}
int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    try
    {
        if (argc != 2) throw std::runtime_error("Usage: " + std::string(argv[0]) + " <path-to-json-array-file>");
        JsonBlockPager pager(argv[1]);
        auto progressCallback = [](int pct) {
            std::cout << "\rIndexing... " << pct << "%" << std::flush;
        };
        pager.calculateTotalBlocks(progressCallback);
        std::cout << "\rIndexing done.    \n" << std::flush;
        LinePager lp(pager);
        render(lp);
        while (true)
        {
            const Key key = readKey();
            if (key == Key::Unknown) continue;
            if (key == Key::Quit)    return EXIT_SUCCESS;
            const int rows = terminalRows();
            switch (key)
            {
            case Key::ArrowUp:   lp.scroll(-1);          break;
            case Key::ArrowDown: lp.scroll(+1);          break;
            case Key::PageUp:    lp.scroll(-(rows - 2)); break;
            case Key::PageDown:  lp.scroll(+(rows - 2)); break;
            default: break;
            }
            render(lp);
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}