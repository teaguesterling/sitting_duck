package com.example.test;

import java.util.List;
import java.util.ArrayList;
import java.util.stream.Collectors;

/**
 * Test Java class demonstrating various language features
 */
public class Test {
    private static final String CONSTANT = "Hello";
    private int counter = 0;
    
    public Test() {
        this.counter = 0;
    }
    
    public Test(int initialValue) {
        this.counter = initialValue;
    }
    
    @Override
    public String toString() {
        return "Test{counter=" + counter + "}";
    }
    
    public void increment() {
        counter++;
    }
    
    protected int getCounter() {
        return counter;
    }
    
    private void reset() {
        counter = 0;
    }
    
    public static void main(String[] args) {
        Test test = new Test(10);
        test.increment();
        
        List<Integer> numbers = new ArrayList<>();
        for (int i = 0; i < 10; i++) {
            numbers.add(i);
        }
        
        List<Integer> evenNumbers = numbers.stream()
            .filter(n -> n % 2 == 0)
            .collect(Collectors.toList());
            
        System.out.println("Even numbers: " + evenNumbers);
        
        try {
            String result = processData(null);
        } catch (IllegalArgumentException e) {
            System.err.println("Error: " + e.getMessage());
        }
    }
    
    private static String processData(String input) throws IllegalArgumentException {
        if (input == null) {
            throw new IllegalArgumentException("Input cannot be null");
        }
        return input.toUpperCase();
    }
    
    enum Status {
        ACTIVE, INACTIVE, PENDING
    }
    
    interface Processor {
        void process(String data);
    }
    
    static class InnerClass {
        private String name;
        
        public InnerClass(String name) {
            this.name = name;
        }
    }
}