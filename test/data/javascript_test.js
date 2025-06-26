// JavaScript test file for native context extraction

// Basic function declaration
function basicFunction(arg1, arg2 = "default") {
    return arg1 + arg2;
}

// Arrow function with parameters
const arrowFunction = (x, y) => x + y;

// Arrow function with complex parameters
const complexArrow = (name, { age = 18, active = true } = {}) => {
    return `${name} is ${age} years old`;
};

// Async function
async function asyncFunction(data, timeout = 5000) {
    return new Promise(resolve => setTimeout(() => resolve(data), timeout));
}

// Async arrow function
const asyncArrow = async (items) => {
    return items.map(item => item.toString());
};

// Class with constructor and methods
class ExampleClass {
    constructor(name, value = 0) {
        this.name = name;
        this.value = value;
    }
    
    instanceMethod(multiplier) {
        return this.value * multiplier;
    }
    
    static staticMethod(x, y) {
        return x + y;
    }
    
    async asyncMethod(data) {
        return await this.processData(data);
    }
    
    get computedValue() {
        return this.value * 2;
    }
    
    set updateValue(newValue) {
        this.value = newValue;
    }
}

// Class with inheritance
class ChildClass extends ExampleClass {
    constructor(name, value = 0, extra = "") {
        super(name, value);
        this.extra = extra;
    }
    
    childMethod(param) {
        return super.instanceMethod(param) + this.extra.length;
    }
}

// Function with destructuring parameters
function destructuringParams({ name, age }, [x, y] = [0, 0]) {
    return { name, age, sum: x + y };
}

// Generator function
function* generatorFunction(items) {
    for (let item of items) {
        yield item.toString();
    }
}

// Higher-order function
function higherOrderFunction(callback, ...args) {
    return callback(...args);
}

// IIFE (Immediately Invoked Function Expression)
(function(global) {
    global.myModule = {
        helper: function(data) {
            return data;
        }
    };
})(this);

// Variable declarations
const constantVariable = "constant";
let mutableVariable = 42;
var legacyVariable = true;