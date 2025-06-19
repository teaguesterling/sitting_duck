<?php
// Simple PHP test file for AST parsing
namespace App\Models;

use DateTime;
use Exception;

/**
 * User class for testing PHP AST parsing
 */
class User {
    private int $id;
    private string $name;
    private string $email;
    public DateTime $created_at;
    
    public function __construct(int $id, string $name, string $email) {
        $this->id = $id;
        $this->name = $name;
        $this->email = $email;
        $this->created_at = new DateTime();
    }
    
    public function getName(): string {
        return $this->name;
    }
    
    public function getEmail(): string {
        return $this->email;
    }
    
    private function validateEmail(): bool {
        return filter_var($this->email, FILTER_VALIDATE_EMAIL) !== false;
    }
    
    public function isValid(): bool {
        return !empty($this->name) && $this->validateEmail();
    }
}

interface Validator {
    public function validate($data): bool;
}

class EmailValidator implements Validator {
    public function validate($data): bool {
        return filter_var($data, FILTER_VALIDATE_EMAIL) !== false;
    }
}

function createUser(string $name, string $email): User {
    if (empty($name)) {
        throw new Exception("Name cannot be empty");
    }
    
    $user = new User(1, $name, $email);
    
    if (!$user->isValid()) {
        throw new Exception("Invalid user data");
    }
    
    return $user;
}

function formatName(string $name): string {
    return trim(strtolower($name));
}

// Test constants
define('DEFAULT_TIMEOUT', 30);
const MAX_USERS = 1000;

// Test array
$users = [
    'john' => createUser('John Doe', 'john@example.com'),
    'jane' => createUser('Jane Smith', 'jane@example.com')
];

foreach ($users as $key => $user) {
    echo "User: " . $user->getName() . "\n";
}
?>