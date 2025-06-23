# Simple R test file for AST parsing
# This file contains basic R constructs for testing

# Variable assignments
x <- 5
y = 10
z <<- 15

# Function definition
calculate_mean <- function(values, na.rm = TRUE) {
    if (na.rm) {
        values <- values[!is.na(values)]
    }
    return(sum(values) / length(values))
}

# Data structures
my_vector <- c(1, 2, 3, 4, 5)
my_list <- list(
    numbers = my_vector,
    text = "hello world",
    logical = TRUE
)

# Control flow
for (i in 1:10) {
    if (i %% 2 == 0) {
        print(paste("Even:", i))
    } else {
        print(paste("Odd:", i))
    }
}

# While loop
counter <- 1
while (counter <= 5) {
    print(counter)
    counter <- counter + 1
}

# Function calls
result <- calculate_mean(my_vector)
summary(my_list)

# Data manipulation
filtered_data <- subset(mtcars, mpg > 20)
aggregated <- aggregate(mpg ~ cyl, data = mtcars, FUN = mean)

# R-specific operators
piped_result <- mtcars |> 
    subset(mpg > 20) |>
    aggregate(mpg ~ cyl, data = ., FUN = mean)

# Special values
missing_value <- NA
infinite_value <- Inf
complex_number <- 1 + 2i

# Comments and documentation
# This is a single line comment

# Private function (starts with dot)
.private_helper <- function(x) {
    return(x * 2)
}

# Package operations
library(dplyr)
# import::from(magrittr, "%>%")