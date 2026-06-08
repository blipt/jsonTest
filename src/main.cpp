#include "JsonBlockPager.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
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

namespace {

enum class Key {
    PageUp,
    PageDown,
    Quit,
    Unknown,
};

class TerminalRawMode {
public:
    TerminalRawMode() {
#ifndef _WIN32
        if (tcgetattr(STDIN_FILENO, &original_) == 0) {
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

    ~TerminalRawMode() {
#ifndef _WIN32
        if (enabled_) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original_);
        }
#endif
    }

private:
#ifndef _WIN32
    termios original_{};
    bool enabled_{false};
#endif
};

void clearScreen() {
    std::cout << "\033[2J\033[H";
}

void printHelp() {
    std::cout << "Keys: PageDown = next object, PageUp = previous object, q/Esc = quit\n";
}

void renderBlock(const JsonBlockPager& pager, const JsonBlockPager::Block& block) {
    clearScreen();
    std::cout << "File: " << pager.path().string() << '\n';
    std::cout << "Object: " << (block.index + 1) << " (zero-based index " << block.index << ")\n";
    printHelp();
    std::cout << "------------------------------------------------------------\n";
    std::cout << block.raw << "\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout.flush();
}

void renderMessage(const std::string& message) {
    clearScreen();
    std::cout << message << "\n\n";
    printHelp();
    std::cout.flush();
}

#ifndef _WIN32
bool readByteWithTimeout(char& ch, int timeoutMilliseconds) {
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

Key readKey() {
#ifdef _WIN32
    const int ch = _getch();
    if (ch == 'q' || ch == 'Q' || ch == 27) {
        return Key::Quit;
    }
    if (ch == 0 || ch == 224) {
        const int extended = _getch();
        if (extended == 73) {
            return Key::PageUp;
        }
        if (extended == 81) {
            return Key::PageDown;
        }
    }
    return Key::Unknown;
#else
    char ch = '\0';
    if (::read(STDIN_FILENO, &ch, 1) != 1) {
        return Key::Unknown;
    }

    if (ch == 'q' || ch == 'Q') {
        return Key::Quit;
    }

    if (ch != '\033') {
        return Key::Unknown;
    }

    char introducer = '\0';
    if (!readByteWithTimeout(introducer, 50)) {
        return Key::Quit;
    }
    if (introducer != '[') {
        return Key::Unknown;
    }

    std::string sequence;
    while (sequence.size() < 16) {
        char sequenceByte = '\0';
        if (!readByteWithTimeout(sequenceByte, 50)) {
            return Key::Unknown;
        }

        sequence.push_back(sequenceByte);
        if (sequenceByte >= '@' && sequenceByte <= '~') {
            break;
        }
    }

    if (sequence == "5~") {
        return Key::PageUp;
    }
    if (sequence == "6~") {
        return Key::PageDown;
    }
    return Key::Unknown;
#endif
}

void printUsage(const char* executable) {
    std::cerr << "Usage: " << executable << " <path-to-json-array-file>\n";
    std::cerr << "The file must have the top-level shape: [{...},{...},...]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    try {
        JsonBlockPager pager(argv[1]);
        TerminalRawMode rawMode;

        std::optional<JsonBlockPager::Block> block = pager.loadCurrent();
        if (!block) {
            renderMessage("No object blocks were found in the array.");
        } else {
            renderBlock(pager, *block);
        }

        while (true) {
            switch (readKey()) {
            case Key::PageDown: {
                auto next = pager.loadNext();
                if (next) {
                    block = std::move(next);
                    renderBlock(pager, *block);
                } else {
                    renderMessage("End of array. Press PageUp to go back or q to quit.");
                }
                break;
            }
            case Key::PageUp: {
                auto previous = pager.loadPrevious();
                if (previous) {
                    block = std::move(previous);
                    renderBlock(pager, *block);
                } else {
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
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
