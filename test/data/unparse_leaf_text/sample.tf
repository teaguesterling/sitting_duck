# a comment
variable "name" {
  type    = string
  default = "world"
}

resource "demo" "example" {
  count  = 3
  labels = ["a", "b"]
}
