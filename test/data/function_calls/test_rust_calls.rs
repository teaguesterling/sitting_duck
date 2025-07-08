fn foo(x: i32, y: &str) -> String {
    format\!("{}: {}", x, y)
}

fn main() {
    let result = foo(42, "hello");
    let obj = MyStruct::new(10);
}
