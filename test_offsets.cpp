#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <filesystem>

bool isWhitespace(char ch) noexcept {
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

int64_t parseJsonArray(std::ifstream& input, int64_t& objectStart, int64_t fileSize_) {
    if (!input.seekg(static_cast<std::streamoff>(objectStart)))
        return -1;
    
    char ch = '\0';
    while (input.get(ch))
    {
        if (ch == '{')
        {
            int64_t objStartPos = static_cast<int64_t>(input.tellg()) - 1;
            
            int braceCount = 1;
            bool inString = false;
            bool escape = false;
            
            while (braceCount > 0 && input.get(ch))
            {
                if (escape) { escape = false; continue; }
                if (ch == '\\' && inString) { escape = true; continue; }
                if (ch == '"') { inString = !inString; continue; }
                if (!inString) {
                    if (ch == '{') braceCount++;
                    else if (ch == '}') braceCount--;
                }
            }
            
            while (input.get(ch))
            {
                if (ch == '}' || ch == ']') {
                    objectStart = static_cast<int64_t>(input.tellg());
                    return objStartPos;
                }
                if (ch == ',') {
                    objectStart = static_cast<int64_t>(input.tellg());
                    return objStartPos;
                }
                if (!isWhitespace(ch)) break;
            }
            
            objectStart = static_cast<int64_t>(input.tellg());
            if (input.eof()) objectStart = fileSize_;
            return objStartPos;
        }
        else if (ch == ']' || ch == '}')
        {
            objectStart = static_cast<int64_t>(input.tellg());
            if (input.eof()) objectStart = fileSize_;
            return objectStart;
        }
    }
    
    objectStart = fileSize_;
    return objectStart;
}

int main() {
    std::ifstream input("/workspace/test_data.json", std::ios::binary);
    int64_t fileSize_ = std::filesystem::file_size("/workspace/test_data.json");
    
    std::vector<int64_t> offsets;
    int64_t objectStart = 0;
    
    while (true) {
        if (objectStart >= fileSize_) break;
        int64_t nextStart = parseJsonArray(input, objectStart, fileSize_);
        if (nextStart < 0) break;
        offsets.push_back(nextStart);
    }
    
    std::cout << "Offsets:" << std::endl;
    for (size_t i = 0; i < offsets.size(); i++) {
        std::cout << "  [" << i << "] = " << offsets[i] << std::endl;
    }
    std::cout << "Total blocks: " << (offsets.empty() ? 0 : offsets.size() - 1) << std::endl;
    
    // Test loading block 0
    if (offsets.size() >= 2) {
        int64_t start = offsets[0];
        int64_t end = offsets[1];
        std::cout << "\nBlock 0: from " << start << " to " << end << " (length " << (end-start) << ")" << std::endl;
        
        input.clear();
        input.seekg(start);
        std::string buf(end - start, '\0');
        input.read(buf.data(), end - start);
        std::cout << "Content: " << buf << std::endl;
    }
    
    return 0;
}
