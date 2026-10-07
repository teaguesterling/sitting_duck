// line comment
/* block comment */
import Foundation

func greet(name: String) -> String {
    let s = "hi"
    let hex = 0x2a
    let d = 3.5
    return "\(s) \(name) \(hex) \(d)"
}

print(greet(name: "world"))
