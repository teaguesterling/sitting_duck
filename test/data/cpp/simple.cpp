// Simple C++ test file

#include <iostream>
#include <vector>
#include <memory>

namespace utils {
    template<typename T>
    class Container {
    public:
        Container() : size_(0) {}
        
        void add(const T& item) {
            items_.push_back(item);
            size_++;
        }
        
        T get(size_t index) const {
            return items_[index];
        }
        
        size_t size() const { return size_; }
        
    private:
        std::vector<T> items_;
        size_t size_;
    };
}

class Calculator {
public:
    Calculator() : result_(0) {}
    
    int add(int a, int b) {
        result_ = a + b;
        return result_;
    }
    
    int multiply(int x, int y) {
        return x * y;
    }
    
    // Operator overloading
    Calculator& operator+=(int value) {
        result_ += value;
        return *this;
    }
    
private:
    int result_;
};

// Function template
template<typename T>
T max(T a, T b) {
    return (a > b) ? a : b;
}

// Free function
void printMessage(const std::string& msg) {
    std::cout << "Message: " << msg << std::endl;
}

int main() {
    // Using the Calculator class
    Calculator calc;
    int sum = calc.add(5, 3);
    
    // Using the template class
    utils::Container<int> numbers;
    numbers.add(10);
    numbers.add(20);
    
    // Using function template
    int maxInt = max(10, 20);
    double maxDouble = max(3.14, 2.71);
    
    // Lambda expression
    auto multiply = [](int a, int b) -> int {
        return a * b;
    };
    
    int product = multiply(4, 5);
    
    return 0;
}