#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

class JsonBlockPager {
public:
    struct Block {
        std::size_t index{};
        std::string raw;
    };

    explicit JsonBlockPager(const std::filesystem::path& path);

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] std::size_t currentIndex() const noexcept;
    [[nodiscard]] bool hasCurrent() const noexcept;

    std::optional<Block> loadCurrent();
    std::optional<Block> loadNext();
    std::optional<Block> loadPrevious();

private:
    enum class ScanResult {
        FoundObject,
        EndOfArray,
        EndOfFile,
    };

    using Offset = std::uint64_t;

    std::optional<Block> loadAt(std::size_t index);
    ScanResult findNextObjectStart(Offset& objectStart);
    std::string readObjectRaw(Offset objectStart, Offset& afterObject);
    char readChar();
    void seek(Offset offset);
    [[nodiscard]] Offset tell();
    static bool isWhitespace(char ch) noexcept;

    std::filesystem::path path_;
    std::ifstream input_;
    std::vector<Offset> objectOffsets_;
    Offset scanOffset_{0};
    std::size_t currentIndex_{0};
    bool hasCurrent_{false};
    bool reachedEnd_{false};
};
