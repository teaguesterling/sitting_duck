#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_SIZE 100
#define SQUARE(x) ((x) * (x))

// Type definitions
typedef struct Node {
    int data;
    struct Node* next;
} Node;

typedef enum {
    SUCCESS = 0,
    ERROR_NULL_POINTER = -1,
    ERROR_OUT_OF_MEMORY = -2,
    ERROR_INVALID_INPUT = -3
} ErrorCode;

// Global variables
static int global_counter = 0;
const char* VERSION = "1.0.0";

// Function declarations
static Node* create_node(int value);
void insert_node(Node** head, int value);
bool find_node(const Node* head, int value);
void free_list(Node* head);
static inline int max(int a, int b);

// Main function
int main(int argc, char* argv[]) {
    printf("C Test Program v%s\n", VERSION);
    
    // Variable declarations
    int numbers[] = {1, 2, 3, 4, 5};
    int count = sizeof(numbers) / sizeof(numbers[0]);
    char buffer[MAX_SIZE];
    
    // Control structures
    for (int i = 0; i < count; i++) {
        printf("Number %d: %d\n", i, numbers[i]);
    }
    
    // Pointer arithmetic
    int* ptr = numbers;
    while (ptr < numbers + count) {
        *ptr = SQUARE(*ptr);
        ptr++;
    }
    
    // Linked list example
    Node* list = NULL;
    insert_node(&list, 10);
    insert_node(&list, 20);
    insert_node(&list, 30);
    
    if (find_node(list, 20)) {
        puts("Found 20 in the list");
    }
    
    // String manipulation
    strcpy(buffer, "Hello, ");
    strcat(buffer, "World!");
    printf("%s (length: %zu)\n", buffer, strlen(buffer));
    
    // Dynamic memory allocation
    int* dynamic_array = (int*)calloc(10, sizeof(int));
    if (dynamic_array == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_list(list);
        return ERROR_OUT_OF_MEMORY;
    }
    
    // Bit manipulation
    unsigned int flags = 0;
    flags |= (1 << 2);  // Set bit 2
    flags &= ~(1 << 1); // Clear bit 1
    
    // Conditional operator
    int result = (argc > 1) ? atoi(argv[1]) : 42;
    printf("Result: %d\n", result);
    
    // Switch statement
    switch (result % 3) {
        case 0:
            puts("Divisible by 3");
            break;
        case 1:
            puts("Remainder 1");
            break;
        case 2:
            puts("Remainder 2");
            break;
        default:
            puts("Unexpected");
    }
    
    // Cleanup
    free(dynamic_array);
    free_list(list);
    
    return SUCCESS;
}

// Function implementations
static Node* create_node(int value) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node != NULL) {
        new_node->data = value;
        new_node->next = NULL;
        global_counter++;
    }
    return new_node;
}

void insert_node(Node** head, int value) {
    if (head == NULL) return;
    
    Node* new_node = create_node(value);
    if (new_node == NULL) return;
    
    new_node->next = *head;
    *head = new_node;
}

bool find_node(const Node* head, int value) {
    const Node* current = head;
    while (current != NULL) {
        if (current->data == value) {
            return true;
        }
        current = current->next;
    }
    return false;
}

void free_list(Node* head) {
    Node* current = head;
    while (current != NULL) {
        Node* next = current->next;
        free(current);
        global_counter--;
        current = next;
    }
}

static inline int max(int a, int b) {
    return (a > b) ? a : b;
}

// Preprocessor conditionals
#ifdef DEBUG
void debug_print(const char* msg) {
    fprintf(stderr, "[DEBUG] %s\n", msg);
}
#endif

// Function pointer example
typedef int (*operation_t)(int, int);

int add(int a, int b) { return a + b; }
int subtract(int a, int b) { return a - b; }

int perform_operation(int x, int y, operation_t op) {
    return op(x, y);
}