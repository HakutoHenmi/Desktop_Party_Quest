#include <iostream>
#include <fstream>

int main() {
    std::ifstream file("Resources/Textures/Characters/char_tank.png", std::ios::binary);
    if (!file) {
        std::cout << "File not found" << std::endl;
        return 1;
    }
    char buf[8];
    file.read(buf, 8);
    for (int i = 0; i < 8; ++i) {
        std::cout << std::hex << (int)(unsigned char)buf[i] << " ";
    }
    std::cout << std::endl;
    return 0;
}
