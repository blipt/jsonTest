#include <iostream>
#include <fstream>
#include <cstdint>

int main() {
    std::ifstream input("/workspace/test_data.json", std::ios::binary);
    if (!input) {
        std::cerr << "Cannot open file" << std::endl;
        return 1;
    }
    
    // Get file size
    input.seekg(0, std::ios::end);
    int64_t fileSize = static_cast<int64_t>(input.tellg());
    std::cout << "File size: " << fileSize << std::endl;
    
    // Read and print first 100 chars
    input.seekg(0);
    char buf[101] = {0};
    input.read(buf, 100);
    std::cout << "First 100 chars: '" << buf << "'" << std::endl;
    
    // Try to find '{'
    input.seekg(0);
    char ch;
    int pos = 0;
    while (input.get(ch)) {
        if (ch == '{') {
            std::cout << "Found '{' at position " << pos << std::endl;
            std::cout << "tellg after get: " << input.tellg() << std::endl;
        }
        pos++;
    }
    
    return 0;
}
