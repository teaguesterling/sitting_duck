// TypeScript test file for native context extraction

// Interface definition
interface User {
    name: string;
    age: number;
    active?: boolean;
}

// Type alias
type ProcessorFunction = (data: string[]) => Promise<string>;

// Basic function with TypeScript types
function basicTypedFunction(name: string, age: number = 18): User {
    return { name, age, active: true };
}

// Arrow function with generic types
const genericArrowFunction = <T>(items: T[], predicate: (item: T) => boolean): T[] => {
    return items.filter(predicate);
};

// Async function with TypeScript types
async function asyncTypedFunction(data: string[], timeout: number = 5000): Promise<string[]> {
    return new Promise(resolve => 
        setTimeout(() => resolve(data.map(s => s.toUpperCase())), timeout)
    );
}

// Class with TypeScript features
class TypedExampleClass implements User {
    public name: string;
    private _age: number;
    protected value: number;
    readonly id: string;
    
    constructor(name: string, age: number = 0, value: number = 0) {
        this.name = name;
        this._age = age;
        this.value = value;
        this.id = Math.random().toString(36);
    }
    
    get age(): number {
        return this._age;
    }
    
    set age(newAge: number) {
        if (newAge >= 0) {
            this._age = newAge;
        }
    }
    
    get active(): boolean {
        return this._age > 0;
    }
    
    public instanceMethod(multiplier: number): number {
        return this.value * multiplier;
    }
    
    public static staticMethod<T>(items: T[]): number {
        return items.length;
    }
    
    protected protectedMethod(data: string): string {
        return data.toLowerCase();
    }
    
    private privateMethod(): void {
        // Private implementation
    }
    
    async asyncMethod(processor: ProcessorFunction): Promise<string> {
        return await processor([this.name]);
    }
}

// Abstract class
abstract class AbstractBase {
    abstract abstractMethod(param: string): number;
    
    public concreteMethod(value: number): string {
        return value.toString();
    }
}

// Generic class
class GenericContainer<T> {
    private items: T[] = [];
    
    public add(item: T): void {
        this.items.push(item);
    }
    
    public get(index: number): T | undefined {
        return this.items[index];
    }
    
    public getAll(): readonly T[] {
        return this.items;
    }
}

// Function with union types and optional parameters
function unionTypeFunction(
    value: string | number, 
    options?: { format?: 'json' | 'xml'; validate?: boolean }
): string {
    if (typeof value === 'string') {
        return value;
    }
    return value.toString();
}

// Function with rest parameters and tuple types
function restParameterFunction(
    first: string, 
    ...rest: [number, boolean, string?]
): { first: string; rest: any[] } {
    return { first, rest };
}

// Decorator function (if experimental decorators are enabled)
function logMethod(target: any, propertyName: string, descriptor: PropertyDescriptor) {
    const method = descriptor.value;
    descriptor.value = function (...args: any[]) {
        console.log(`Calling ${propertyName} with`, args);
        return method.apply(this, args);
    };
}

// Class with decorator
class DecoratedClass {
    @logMethod
    public decoratedMethod(param: string): string {
        return param.toUpperCase();
    }
}

// Enum
enum Color {
    Red = "red",
    Green = "green",
    Blue = "blue"
}

// Namespace
namespace Utilities {
    export function formatName(first: string, last: string): string {
        return `${first} ${last}`;
    }
    
    export const defaultTimeout = 5000;
}

// Module declaration
declare module "external-library" {
    export function externalFunction(param: string): number;
}

// Variable declarations with explicit types
const typedConstant: string = "constant";
let typedVariable: number = 42;
const complexType: Map<string, User[]> = new Map();
const tupleType: [string, number, boolean] = ["test", 42, true];