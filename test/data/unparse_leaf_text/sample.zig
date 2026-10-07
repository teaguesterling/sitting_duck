// line comment
const std = @import("std");

pub fn main() void {
    const s = "hi";
    const hex: u8 = 0x2a;
    const d: f64 = 3.5;
    std.debug.print("{s} {d} {d}\n", .{ s, hex, d });
}
