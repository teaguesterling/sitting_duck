// Test fixture for CSS selector coverage

function greet(name) {
    console.log("Hello, " + name);
}

function add(a, b) {
    return a + b;
}

class Shape {
    constructor(color) {
        this.color = color;
    }

    area() {
        return 0;
    }

    describe() {
        return "A " + this.color + " shape";
    }
}

class Circle extends Shape {
    constructor(color, radius) {
        super(color);
        this.radius = radius;
    }

    area() {
        return Math.PI * this.radius * this.radius;
    }
}

class Square extends Shape {
    constructor(color, side) {
        super(color);
        this.side = side;
    }

    area() {
        return this.side * this.side;
    }

    perimeter() {
        return 4 * this.side;
    }
}

async function fetchShape(url) {
    try {
        const response = await fetch(url);
        const data = await response.json();
        return data;
    } catch (error) {
        console.error("Failed:", error);
        return null;
    }
}

function processShapes(shapes) {
    for (const shape of shapes) {
        if (shape.area() > 0) {
            console.log(shape.describe());
        }
    }
}
