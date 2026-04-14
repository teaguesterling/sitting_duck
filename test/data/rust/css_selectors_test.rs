// Test fixture for CSS selector coverage

pub struct Point {
    x: f64,
    y: f64,
}

impl Point {
    pub fn new(x: f64, y: f64) -> Self {
        Point { x, y }
    }

    pub fn distance(&self, other: &Point) -> f64 {
        ((self.x - other.x).powi(2) + (self.y - other.y).powi(2)).sqrt()
    }

    fn is_origin(&self) -> bool {
        self.x == 0.0 && self.y == 0.0
    }
}

pub trait Drawable {
    fn draw(&self);
    fn visible(&self) -> bool;
}

impl Drawable for Point {
    fn draw(&self) {
        println!("Point({}, {})", self.x, self.y);
    }

    fn visible(&self) -> bool {
        true
    }
}

pub fn create_point(x: f64, y: f64) -> Point {
    Point::new(x, y)
}

fn helper(val: f64) -> f64 {
    if val > 0.0 {
        val * 2.0
    } else {
        0.0
    }
}

pub mod geometry {
    pub fn area(width: f64, height: f64) -> f64 {
        width * height
    }

    pub fn perimeter(width: f64, height: f64) -> f64 {
        2.0 * (width + height)
    }
}
