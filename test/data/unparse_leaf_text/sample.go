// line comment
package main

/* block comment */
import "fmt"

func main() {
	s := "hi\n"
	raw := `raw string`
	hex := 0x2a
	fmt.Println(s, raw, hex)
}
