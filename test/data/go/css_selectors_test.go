package geometry

import "fmt"

type Point struct {
	X float64
	Y float64
}

func NewPoint(x, y float64) Point {
	return Point{X: x, Y: y}
}

func (p Point) Distance(other Point) float64 {
	dx := p.X - other.X
	dy := p.Y - other.Y
	return dx*dx + dy*dy
}

func (p Point) String() string {
	return fmt.Sprintf("(%f, %f)", p.X, p.Y)
}

type Shape interface {
	Area() float64
	Perimeter() float64
}

type Rectangle struct {
	Width  float64
	Height float64
}

func (r Rectangle) Area() float64 {
	return r.Width * r.Height
}

func (r Rectangle) Perimeter() float64 {
	return 2 * (r.Width + r.Height)
}

func ProcessShapes(shapes []Shape) {
	for _, shape := range shapes {
		if shape.Area() > 0 {
			fmt.Println(shape.Area())
		}
	}
}

func Helper(val float64) float64 {
	if val > 0 {
		return val * 2
	}
	return 0
}
