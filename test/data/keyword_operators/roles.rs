// #231 fixture: one `as` token, two roles -- use rename and cast.
use std::io as stdio;
fn f(x: i64) -> u32 {
    let y = x as u32;
    y
}
