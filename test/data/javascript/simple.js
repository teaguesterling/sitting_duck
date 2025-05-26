// Simple JavaScript test file

function hello(name) {
    console.log(`Hello, ${name}!`);
}

class Calculator {
    constructor() {
        this.result = 0;
    }
    
    add(a, b) {
        return a + b;
    }
    
    subtract(a, b) {
        return a - b;
    }
}

const calc = new Calculator();
const sum = calc.add(5, 3);

// Arrow function
const multiply = (x, y) => x * y;

// Object literal
const person = {
    name: "John",
    age: 30,
    greet: function() {
        return `Hi, I'm ${this.name}`;
    }
};

// Array methods
const numbers = [1, 2, 3, 4, 5];
const doubled = numbers.map(n => n * 2);

// Async function
async function fetchData(url) {
    try {
        const response = await fetch(url);
        return await response.json();
    } catch (error) {
        console.error('Error:', error);
    }
}

// Export
export { hello, Calculator, multiply };