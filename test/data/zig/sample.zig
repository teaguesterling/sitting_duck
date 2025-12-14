//! Top-level doc comment for the module
const std = @import("std");
const math = std.math;

/// A simple Point struct
pub const Point = struct {
    x: f32,
    y: f32,
    
    /// Calculate distance from origin
    pub fn distanceFromOrigin(self: Point) f32 {
        return math.sqrt(self.x * self.x + self.y * self.y);
    }
    
    pub fn add(self: Point, other: Point) Point {
        return Point{
            .x = self.x + other.x,
            .y = self.y + other.y,
        };
    }
};

/// Color enum with RGB values
pub const Color = enum {
    red,
    green,
    blue,
    
    pub fn toRGB(self: Color) u32 {
        return switch (self) {
            .red => 0xFF0000,
            .green => 0x00FF00,
            .blue => 0x0000FF,
        };
    }
};

/// Tagged union for shapes
pub const Shape = union(enum) {
    circle: f32,
    rectangle: struct { width: f32, height: f32 },
    
    pub fn area(self: Shape) f32 {
        return switch (self) {
            .circle => |radius| math.pi * radius * radius,
            .rectangle => |rect| rect.width * rect.height,
        };
    }
};

/// Error set for file operations
pub const FileError = error{
    NotFound,
    AccessDenied,
    InvalidPath,
};

// Global constant
const MAX_SIZE: usize = 1024;

// Mutable variable
var global_counter: u32 = 0;

/// Main entry point
pub fn main() !void {
    const allocator = std.heap.page_allocator;
    
    // Variable declarations
    var x: i32 = 42;
    const y: i32 = 100;
    
    // Control flow
    if (x > 0) {
        x += 1;
    } else {
        x = 0;
    }
    
    // Loop
    var i: u32 = 0;
    while (i < 10) : (i += 1) {
        if (i == 5) continue;
        if (i == 8) break;
    }
    
    // For loop
    const items = [_]i32{ 1, 2, 3, 4, 5 };
    for (items) |item| {
        _ = item;
    }
    
    // Error handling with try
    const file = try std.fs.cwd().openFile("test.txt", .{});
    defer file.close();
    
    // Builtin functions
    const size = @sizeOf(Point);
    _ = @intCast(i64, x);
    
    // Comptime
    comptime {
        const compile_time_value = 42 * 2;
        _ = compile_time_value;
    }
    
    _ = allocator;
    _ = y;
    _ = size;
}

/// Generic function
pub fn swap(comptime T: type, a: *T, b: *T) void {
    const temp = a.*;
    a.* = b.*;
    b.* = temp;
}

/// Async function example
pub fn asyncOperation() callconv(.Async) void {
    suspend {}
}

test "basic test" {
    const result = 2 + 2;
    try std.testing.expect(result == 4);
}

test "point operations" {
    const p1 = Point{ .x = 3.0, .y = 4.0 };
    const p2 = Point{ .x = 1.0, .y = 2.0 };
    const p3 = p1.add(p2);
    try std.testing.expect(p3.x == 4.0);
}
