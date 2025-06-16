<?php

namespace App\Test;

use App\Core\BaseClass;
use App\Utils\Helper;

/**
 * Test PHP class demonstrating various language features
 */
class TestClass extends BaseClass implements TestInterface {
    private const VERSION = '1.0.0';
    
    private string $name;
    protected int $count = 0;
    public ?array $data = null;
    
    public function __construct(string $name) {
        $this->name = $name;
        parent::__construct();
    }
    
    public function __destruct() {
        echo "Cleaning up {$this->name}\n";
    }
    
    public function getName(): string {
        return $this->name;
    }
    
    private function validate(mixed $input): bool {
        return !empty($input);
    }
    
    protected function process(array $items): array {
        return array_filter($items, function($item) {
            return $this->validate($item);
        });
    }
    
    public static function createFromArray(array $data): self {
        return new self($data['name'] ?? 'default');
    }
}

trait LoggerTrait {
    public function log(string $message): void {
        echo "[LOG] $message\n";
    }
}

interface TestInterface {
    public function getName(): string;
}

enum Status: string {
    case ACTIVE = 'active';
    case INACTIVE = 'inactive';
    case PENDING = 'pending';
}

// Global function
function processData($input) {
    if (is_array($input)) {
        return array_map('strtoupper', $input);
    }
    return strtoupper($input);
}

// Anonymous function
$multiply = fn($a, $b) => $a * $b;

// Control structures
$value = 10;
if ($value > 5) {
    echo "Value is greater than 5\n";
} elseif ($value < 0) {
    echo "Value is negative\n";
} else {
    echo "Value is between 0 and 5\n";
}

foreach ([1, 2, 3] as $number) {
    echo "Number: $number\n";
}

try {
    $result = 10 / 0;
} catch (DivisionByZeroError $e) {
    echo "Error: " . $e->getMessage() . "\n";
} finally {
    echo "Cleanup\n";
}

// Match expression (PHP 8+)
$status = Status::ACTIVE;
$message = match($status) {
    Status::ACTIVE => 'System is running',
    Status::INACTIVE => 'System is stopped',
    Status::PENDING => 'System is starting',
};

// Heredoc
$longText = <<<EOT
This is a long text
that spans multiple lines
using heredoc syntax.
EOT;

// Shell execution
$files = `ls -la`;

?>