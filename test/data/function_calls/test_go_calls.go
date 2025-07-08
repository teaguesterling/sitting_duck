package main

func foo(x int, y string) string {
    return fmt.Sprintf("%d: %s", x, y)
}

func main() {
    result := foo(42, "hello")
    obj := NewMyStruct(10)
}
