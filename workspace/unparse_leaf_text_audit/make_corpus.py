#!/usr/bin/env python3
"""Write the per-language leaf-text round-trip corpus.

Each file is small, syntactically valid, and deliberately exercises the node
classes that carry text in a *named* leaf -- the ones tracker 047 "4a" is
about: comments, string bodies and escapes, numeric literals in several bases,
and operators that the grammar exposes as named nodes.

Kept as a generator rather than 25 hand-edited files so the corpus can be
regrown or extended uniformly. Output goes to test/data/unparse_leaf_text/.
"""

from __future__ import annotations

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = REPO_ROOT / "test" / "data" / "unparse_leaf_text"

CORPUS: dict[str, tuple[str, str]] = {
    "bash": ("sample.sh", """\
#!/usr/bin/env bash
# leading comment
set -eu
name="world"
printf '%s\\n' "hello ${name}"
count=42
if [ "$count" -gt 7 ]; then
  echo 'single quoted'
fi
"""),
    "c": ("sample.c", """\
/* block comment */
#include <stdio.h>

// line comment
int main(void) {
    const char *s = "hi\\n";
    int hex = 0x2a;
    double d = 3.5;
    printf("%s %d %f", s, hex, d);
    return 0;
}
"""),
    "cpp": ("sample.cpp", """\
/* block comment */
#include <string>

// line comment
namespace demo {
auto greet() -> std::string {
    auto s = std::string("hi\\n");
    int hex = 0x2a;
    return s;
}
} // namespace demo
"""),
    "csharp": ("sample.cs", """\
// line comment
using System;

/* block comment */
namespace Demo {
    class Program {
        static void Main() {
            string s = "hi\\n";
            int hex = 0x2a;
            Console.WriteLine(s + hex);
        }
    }
}
"""),
    "css": ("sample.css", """\
/* a comment */
.selector {
    color: #ff0000;
    content: "quoted";
    margin: 10px;
}

#ident > .child {
    width: 50%;
}
"""),
    "dart": ("sample.dart", """\
// line comment
/* block comment */
void main() {
  var s = 'hi';
  var t = "there";
  int hex = 0x2a;
  double d = 3.5;
  print('$s $t $hex $d');
}
"""),
    "go": ("sample.go", """\
// line comment
package main

/* block comment */
import "fmt"

func main() {
	s := "hi\\n"
	raw := `raw string`
	hex := 0x2a
	fmt.Println(s, raw, hex)
}
"""),
    "graphql": ("sample.graphql", """\
# a comment
query GetUser($id: ID!) {
  user(id: $id) {
    name
    age
    tags
  }
}
"""),
    "hcl": ("sample.tf", """\
# a comment
variable "name" {
  type    = string
  default = "world"
}

resource "demo" "example" {
  count  = 3
  labels = ["a", "b"]
}
"""),
    "html": ("sample.html", """\
<!-- a comment -->
<html>
  <head><title>Title</title></head>
  <body>
    <p class="greeting">hello</p>
  </body>
</html>
"""),
    "java": ("sample.java", """\
// line comment
/* block comment */
public class Sample {
    public static void main(String[] args) {
        String s = "hi\\n";
        int hex = 0x2a;
        double d = 3.5;
        System.out.println(s + hex + d);
    }
}
"""),
    "javascript": ("sample.js", """\
// line comment
/* block comment */
const s = "hi\\n";
const t = 'single';
const hex = 0x2a;
const d = 3.5;

function greet(name) {
  return `hello ${name}`;
}

greet(s);
"""),
    "json": ("sample.json", """\
{
  "name": "world",
  "count": 42,
  "ratio": 3.5,
  "flag": true,
  "empty": null,
  "list": [1, 2, 3]
}
"""),
    "kotlin": ("sample.kt", """\
// line comment
/* block comment */
package demo

fun main() {
    val s = "hi"
    val hex = 0x2a
    val d = 3.5
    println("$s $hex $d")
}
"""),
    "lua": ("sample.lua", """\
-- line comment
--[[ block comment ]]
local s = "hi"
local t = 'single'
local hex = 0x2a
local d = 3.5

local function greet(name)
  return "hello " .. name
end

print(greet(s), t, hex, d)
"""),
    "markdown": ("sample.md", """\
# Heading

Some paragraph text with `code` in it.

- list item one
- list item two

```python
x = 1
```
"""),
    "php": ("sample.php", """\
<?php
// line comment
/* block comment */
$s = "hi\\n";
$t = 'single';
$hex = 0x2a;
$d = 3.5;

function greet($name) {
    return "hello " . $name;
}

echo greet($s);
"""),
    "python": ("sample.py", """\
# leading comment
import os


def greet(name: str) -> str:
    \"\"\"Docstring.\"\"\"
    # inner comment
    prefix = "hello"
    count = 42
    ratio = 3.5
    return f"{prefix} {name} {count} {ratio}"


print(greet(os.name))
"""),
    "r": ("sample.R", """\
# a comment
s <- "hi"
t <- 'single'
count <- 42L
ratio <- 3.5

greet <- function(name) {
  paste("hello", name)
}

print(greet(s))
"""),
    "ruby": ("sample.rb", """\
# a comment
s = "hi"
t = 'single'
hex = 0x2a
d = 3.5

def greet(name)
  "hello #{name}"
end

puts greet(s)
"""),
    "rust": ("sample.rs", """\
// line comment
/* block comment */
/// doc comment
fn greet(name: &str) -> String {
    let s = "hi\\n";
    let hex = 0x2a;
    let d = 3.5;
    format!("{} {} {} {}", s, name, hex, d)
}

fn main() {
    println!("{}", greet("world"));
}
"""),
    "swift": ("sample.swift", """\
// line comment
/* block comment */
import Foundation

func greet(name: String) -> String {
    let s = "hi"
    let hex = 0x2a
    let d = 3.5
    return "\\(s) \\(name) \\(hex) \\(d)"
}

print(greet(name: "world"))
"""),
    "toml": ("sample.toml", """\
# a comment
name = "world"
count = 42
ratio = 3.5
flag = true

[section]
list = [1, 2, 3]
nested = { key = "value" }
"""),
    "typescript": ("sample.ts", """\
// line comment
/* block comment */
interface Greeting {
  name: string;
  count: number;
}

const g: Greeting = { name: "world", count: 42 };

function greet(input: Greeting): string {
  return `hello ${input.name} ${input.count}`;
}

greet(g);
"""),
    "zig": ("sample.zig", """\
// line comment
const std = @import("std");

pub fn main() void {
    const s = "hi";
    const hex: u8 = 0x2a;
    const d: f64 = 3.5;
    std.debug.print("{s} {d} {d}\\n", .{ s, hex, d });
}
"""),
    "sql": ("sample.sql", """\
-- a comment
SELECT a, b
FROM t
WHERE x = 1
  AND y > 3.5
ORDER BY a DESC
LIMIT 10;
"""),
}


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for language, (filename, content) in sorted(CORPUS.items()):
        path = OUT_DIR / filename
        path.write_text(content, encoding="utf-8")
        print(f"{language:<12} {path.relative_to(REPO_ROOT)}  {len(content)} bytes")
    print(f"\n{len(CORPUS)} files -> {OUT_DIR.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
