// TypeScript example with comprehensive type patterns

interface User {
    id: number;
    name: string;
    email?: string;
}

interface Repository<T> {
    findById(id: number): Promise<T | null>;
    save(entity: T): Promise<T>;
    delete(id: number): Promise<void>;
}

// Function with typed parameters and return type
function calculateTotal(prices: number[], tax: number = 0.1): number {
    const subtotal = prices.reduce((sum, price) => sum + price, 0);
    return subtotal * (1 + tax);
}

// Generic function
function mapArray<T, U>(items: T[], mapper: (item: T) => U): U[] {
    return items.map(mapper);
}

// Arrow functions with types
const getUserName = (user: User): string => user.name;
const isValidEmail = (email: string): boolean => email.includes('@');

// Class with typed methods
class UserService implements Repository<User> {
    private users: Map<number, User> = new Map();
    
    constructor(private apiUrl: string) {}
    
    async findById(id: number): Promise<User | null> {
        const user = this.users.get(id);
        return user || null;
    }
    
    async save(user: User): Promise<User> {
        this.users.set(user.id, user);
        return user;
    }
    
    async delete(id: number): Promise<void> {
        this.users.delete(id);
    }
    
    // Method with complex return type
    async getUsersWithEmails(): Promise<Array<User & { email: string }>> {
        const users = Array.from(this.users.values());
        return users.filter((user): user is User & { email: string } => 
            user.email !== undefined
        );
    }
    
    // Static method
    static validateUser(user: Partial<User>): user is User {
        return typeof user.id === 'number' && 
               typeof user.name === 'string';
    }
}

// Function with union types
function processValue(value: string | number | boolean): string {
    if (typeof value === 'string') {
        return value.toUpperCase();
    } else if (typeof value === 'number') {
        return value.toString();
    } else {
        return value ? 'true' : 'false';
    }
}

// Generic interface
interface ApiResponse<TData> {
    data: TData;
    success: boolean;
    message?: string;
}

// Function with generic constraints
function processApiResponse<T extends { id: number }>(
    response: ApiResponse<T[]>
): T[] {
    return response.success ? response.data : [];
}

// Async generator
async function* fetchUsersPaginated(
    pageSize: number = 10
): AsyncGenerator<User[], void, undefined> {
    let page = 0;
    let hasMore = true;
    
    while (hasMore) {
        const users = await fetchUsersPage(page, pageSize);
        yield users;
        hasMore = users.length === pageSize;
        page++;
    }
}

// Helper function
async function fetchUsersPage(page: number, size: number): Promise<User[]> {
    // Simulate API call
    return [];
}

// Export with types
export type { User, Repository, ApiResponse };
export { UserService, calculateTotal, mapArray };