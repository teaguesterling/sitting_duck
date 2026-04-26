package main

import "fmt"

func helper() int {
	return 42
}

func process(data string) string {
	result := transform(data)
	helper()
	return result
}

func transform(data string) string {
	return data
}

type Service struct {
	db string
}

func NewService(db string) *Service {
	return &Service{db: db}
}

func (s *Service) Fetch(query string) string {
	return s.db
}

func (s *Service) Run() string {
	data := s.Fetch("SELECT 1")
	return process(data)
}

func main() {
	svc := NewService("db")
	result := svc.Run()
	fmt.Println(result)
}
