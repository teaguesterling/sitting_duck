#include <iostream>

int main() {
    int x = 5;
    int* ptr = &x;         // Address-of operator
    int val = *ptr;        // Dereference operator
    
    // Scope resolution
    std::cout << "Hello" << std::endl;
    
    // Arrow operator
    struct Point { int x, y; };
    Point* p = new Point{1, 2};
    int px = p->x;
    
    // Ternary operator
    int max = (x > 10) ? x : 10;
    
    // Bitwise operators
    int bits = x << 2;     // Left shift
    int mask = x & 0xFF;   // Bitwise AND
    int toggle = x ^ 1;    // XOR
    
    // sizeof operator
    size_t size = sizeof(int);
    
    // Type cast
    double d = static_cast<double>(x);
    
    // Increment/decrement
    x++;
    --x;
    
    return 0;
}