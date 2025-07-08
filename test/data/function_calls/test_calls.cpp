#include <iostream>
#include <string>

class MyClass {
public:
    MyClass(int value) : value_(value) {}
    void print() const {
        std::cout << "Value: " << value_ << std::endl;
    }
private:
    int value_;
};

int main() {
    // Function calls
    std::cout << "Testing function calls" << std::endl;
    
    // Constructor call
    MyClass obj(42);
    
    // Method call
    obj.print();
    
    // Function call with multiple arguments
    std::string result = std::string("Hello, ") + std::string("World\!");
    
    // Delete expression
    MyClass* ptr = new MyClass(100);
    delete ptr;
    
    return 0;
}
EOF < /dev/null
