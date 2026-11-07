// Package gotypes is the #140b fixture. It holds every Go form that declares a
// named type, and the two anonymous forms that must NOT carry a name.
//
// It lives in a SUBDIRECTORY on purpose. Three other tests glob
// `test/data/go/*.go`, which does not recurse, so this file changes the counts
// of exactly one existing test (277's control over `test/data/go/**/*.go`).
package gotypes

// The alias form. tree-sitter-go parses this as `type_alias`, which is a
// DIFFERENT node from `type_spec`, and the name sits in a child
// `type_identifier`.
type Celsius = float64

// A defined type whose body is neither a struct nor an interface.
type Counter int

// The struct form: a `type_spec` that wraps a `struct_type`.
type Pair struct {
	First  int
	Second int
}

// The interface form: a `type_spec` that wraps an `interface_type`.
type Reader interface {
	Read() int
}

// A grouped declaration: one `type_declaration` that holds two `type_spec`s.
type (
	Meters float64
	Grams  float64
)

// An ANONYMOUS struct. The `struct_type` here is the whole construct, not a
// sub-part of a named declaration. Flagging `struct_type` as a constituent
// costs this form its place in `.class`. That cost is deliberate, and section 4
// pins it.
var anonymous struct {
	Z int
}

// `struct{}{}` as a map value, which is how Go spells a set. This is the most
// common anonymous struct in the language.
var marker = map[string]struct{}{"a": {}}
