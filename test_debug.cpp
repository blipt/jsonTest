#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>

bool isWhitespace(char ch) noexcept {
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

int64_t parseJsonArray(std::ifstream& input, int64_t& objectStart) {
    std::cout << "parseJsonArray called with objectStart=" << objectStart << std::endl;
    
    if (!input.seekg(static_cast<std::streamoff>(objectStart))) {
        std::cout << "seekg failed" << std::endl;
        return -1;
    }
    
    char ch = '\0';
    while (input.get(ch)) {
        std::cout << "Read char '" << ch << "' at tellg=" << input.tellg() << std::endl;
        
        if (ch == '{') {
            int64_t objStartPos = static_cast<int64_t>(input.tellg()) - 1;
            std::cout << "Found '{' at position " << objStartPos << std::endl;
            
            int braceCount = 1;
            bool inString = false;
            bool escape = false;
            
            while (braceCount > 0 && input.get(ch)) {
                if (escape) {
                    escape = false;
                    continue;
                }
                
                if (ch == '\\' && inString) {
                    escape = true;
                    continue;
                }
                
                if (ch == '"') {
                    inString = !inString;
                    continue;
                }
                
                if (!inString) {
                    if (ch == '{')
                        braceCount++;
                    else if (ch == '}')
                        braceCount--;
                }
            }
            
            std::cout << "Finished parsing object, now looking for comma or end" << std::endl;
            
            while (input.get(ch)) {
                std::cout << "After object, read char '" << ch << "'" << std::endl;
                if (ch == '}' || ch == ']') {
                    objectStart = static_cast<int64_t>(input.tellg());
                    std::cout << "Found end, returning objStartPos=" << objStartPos << ", new objectStart=" << objectStart << std::endl;
                    return objStartPos;
                }
                if (ch == ',') {
                    objectStart = static_cast<int64_t>(input.tellg());
                    std::cout << "Found comma, returning objStartPos=" << objStartPos << ", new objectStart=" << objectStart << std::endl;
                    return objStartPos;
                }
                if (!isWhitespace(ch)) {
                    std::cout << "Unexpected char, breaking" << std::endl;
                    break;
                }
            }
            
            objectStart = static_cast<int64_t>(input.tellg());
            std::cout << "End of search, returning objStartPos=" << objStartPos << ", new objectStart=" << objectStart << std::endl;
            return objStartPos;
        }
        else if (ch == ']' || ch == '}') {
            objectStart = static_cast<int64_t>(input.tellg());
            std::cout << "Found array end, returning objectStart=" << objectStart << std::endl;
            return objectStart;
        }
    }
    
    objectStart = static_cast<int64_t>(input.tellg());
    std::cout << "EOF reached, returning objectStart=" << objectStart << std::endl;
    if (!input && input.eof())
        return objectStart;
    return -1;
}

int main() {
    std::ifstream input("/workspace/test_data.json", std::ios::binary);
    int64_t fileSize = 341;
    
    std::vector<int64_t> offsets;
    int64_t objectStart = 0;
    
    while (true) {
        if (objectStart >= fileSize) {
            std::cout << "Breaking: objectStart >= fileSize" << std::endl;
            break;
        }
        int64_t nextStart = parseJsonArray(input, objectStart);
        std::cout << "Returned nextStart=" << nextStart << std::endl;
        if (nextStart < 0) {
            std::cout << "Breaking: nextStart < 0" << std::endl;
            break;
        }
        offsets.push_back(nextStart);
        std::cout << "Pushed offset, now objectStart=" << objectStart << std::endl;
    }
    
    std::cout << "\nFinal offsets:" << std::endl;
    for (size_t i = 0; i < offsets.size(); i++) {
        std::cout << "  [" << i << "] = " << offsets[i] << std::endl;
    }
    std::cout << "Total blocks: " << (offsets.empty() ? 0 : offsets.size() - 1) << std::endl;
    
    return 0;
}
