package main

import "fmt"

func helper() int {
	return 42
}

func process(data string) string {
	result := transform(data)
	validated := validate(result)
	fmt.Println("processing")
	return validated
}

func transform(data string) string {
	return data
}

func validate(data string) bool {
	return len(data) > 0
}

type Service struct {
	db string
}

func NewService(db string) *Service {
	return &Service{db: db}
}

func (s *Service) Fetch(query string) []string {
	return s.db.Execute(query)
}

func (s *Service) ProcessAll() []string {
	data := s.Fetch("SELECT 1")
	return transform(data[0])
}

func outer() {
	helper()
	process("test")
}

func main() {
	svc := NewService("db")
	fmt.Println(svc)
}
