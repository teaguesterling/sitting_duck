// line comment
/* block comment */
/// doc comment
fn greet(name: &str) -> String {
    let s = "hi\n";
    let hex = 0x2a;
    let d = 3.5;
    format!("{} {} {} {}", s, name, hex, d)
}

fn main() {
    println!("{}", greet("world"));
}
