#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

class JsonBlockPager {
public:
    explicit JsonBlockPager(const std::filesystem::path& path);
    void calculateTotalBlocks(std::function<void(int progress)> progressCallback) noexcept;
    [[nodiscard]] int64_t totalBlocks() noexcept;
    [[nodiscard]] std::vector<std::string> loadBlock(int64_t index);
private:
    std::ifstream input_;
    std::vector<int64_t> objectOffsets_;
    int64_t fileSize_ = -1;
};
