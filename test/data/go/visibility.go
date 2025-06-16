package main

func PublicFunction() string {
    return "public"
}

func privateFunction() string {
    return "private"
}

type PublicStruct struct {
    PublicField   string
    privateField  string
}