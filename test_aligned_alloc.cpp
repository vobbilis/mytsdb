#include <cstdlib>
#include <iostream>
int main() {
    void* ptr = std::aligned_alloc(32, 256);
    if (ptr) {
        std::cout << "aligned_alloc works" << std::endl;
        std::free(ptr);
    } else {
        std::cout << "aligned_alloc failed" << std::endl;
    }
    return 0;
}
