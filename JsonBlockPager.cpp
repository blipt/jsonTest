#include "JsonBlockPager.hpp"

#include <stdexcept>

JsonBlockPager::JsonBlockPager(const std::filesystem::path& path)
    : path_(path), input_(path, std::ios::binary) {
    if (!input_) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }
}

const std::filesystem::path& JsonBlockPager::path() const noexcept {
    return path_;
}

std::size_t JsonBlockPager::currentIndex() const noexcept {
    return currentIndex_;
}

bool JsonBlockPager::hasCurrent() const noexcept {
    return hasCurrent_;
}

std::optional<JsonBlockPager::Block> JsonBlockPager::loadCurrent() {
    if (hasCurrent_) {
        return loadAt(currentIndex_);
    }
    return loadNext();
}

std::optional<JsonBlockPager::Block> JsonBlockPager::loadNext() {
    const std::size_t targetIndex = hasCurrent_ ? currentIndex_ + 1 : 0;
    return loadAt(targetIndex);
}

std::optional<JsonBlockPager::Block> JsonBlockPager::loadPrevious() {
    if (!hasCurrent_ || currentIndex_ == 0) {
        return std::nullopt;
    }
    return loadAt(currentIndex_ - 1);
}

std::optional<JsonBlockPager::Block> JsonBlockPager::loadAt(std::size_t index) {
    while (objectOffsets_.size() <= index && !reachedEnd_) {
        Offset objectStart = 0;
        const ScanResult scanResult = findNextObjectStart(objectStart);
        if (scanResult != ScanResult::FoundObject) {
            reachedEnd_ = true;
            return std::nullopt;
        }

        Offset afterObject = objectStart;
        readObjectRaw(objectStart, afterObject);
        objectOffsets_.push_back(objectStart);
        scanOffset_ = afterObject;
    }

    if (index >= objectOffsets_.size()) {
        return std::nullopt;
    }

    Offset afterObject = objectOffsets_[index];
    Block block{index, readObjectRaw(objectOffsets_[index], afterObject)};
    currentIndex_ = index;
    hasCurrent_ = true;
    return block;
}

JsonBlockPager::ScanResult JsonBlockPager::findNextObjectStart(Offset& objectStart) {
    seek(scanOffset_);

    bool foundArrayStart = !objectOffsets_.empty();
    if (scanOffset_ > 0) {
        foundArrayStart = true;
    }

    while (input_) {
        const char ch = readChar();
        if (!input_) {
            break;
        }

        if (isWhitespace(ch) || ch == ',') {
            continue;
        }

        if (!foundArrayStart) {
            if (ch == '[') {
                foundArrayStart = true;
                continue;
            }
            throw std::runtime_error("Expected '[' at the beginning of the JSON array");
        }

        if (ch == ']') {
            return ScanResult::EndOfArray;
        }

        if (ch == '{') {
            objectStart = tell() - 1;
            return ScanResult::FoundObject;
        }

        throw std::runtime_error("Expected object block '{' inside the JSON array");
    }

    return ScanResult::EndOfFile;
}

std::string JsonBlockPager::readObjectRaw(Offset objectStart, Offset& afterObject) {
    seek(objectStart);

    std::string raw;
    raw.reserve(4096);

    int depth = 0;
    bool insideString = false;
    bool escaped = false;

    while (input_) {
        const char ch = readChar();
        if (!input_) {
            break;
        }

        raw.push_back(ch);

        if (insideString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                insideString = false;
            }
            continue;
        }

        if (ch == '"') {
            insideString = true;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                afterObject = tell();
                return raw;
            }
        }
    }

    throw std::runtime_error("Unexpected end of file while reading object block");
}

char JsonBlockPager::readChar() {
    char ch = '\0';
    input_.get(ch);
    return ch;
}

void JsonBlockPager::seek(Offset offset) {
    input_.clear();
    input_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input_) {
        throw std::runtime_error("Cannot seek in file");
    }
}

JsonBlockPager::Offset JsonBlockPager::tell() {
    const std::streampos position = input_.tellg();
    if (position < 0) {
        throw std::runtime_error("Cannot read file position");
    }
    return static_cast<Offset>(position);
}

bool JsonBlockPager::isWhitespace(char ch) noexcept {
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}
