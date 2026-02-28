-- Simple Lua test file demonstrating various language constructs

-- Module table
local M = {}

-- Local variable declarations
local name = "Lua"
local version = 5.4
local active = true

-- Global function declaration
function greet(who)
    print("Hello, " .. who .. "!")
end

-- Local function declaration
local function factorial(n)
    if n <= 1 then
        return 1
    end
    return n * factorial(n - 1)
end

-- Module function (dot syntax)
function M.create(name, value)
    return { name = name, value = value }
end

-- Method function (colon syntax)
function M:init(config)
    self.config = config
    return self
end

-- Anonymous function assigned to variable
local transform = function(x)
    return x * 2
end

-- Table constructor with fields
local config = {
    host = "localhost",
    port = 8080,
    debug = false,
}

-- Class-like pattern using metatables
local Animal = {}
Animal.__index = Animal

function Animal.new(name, sound)
    local self = setmetatable({}, Animal)
    self.name = name
    self.sound = sound
    return self
end

function Animal:speak()
    print(self.name .. " says " .. self.sound)
end

-- Control flow
local function process(items)
    local results = {}
    for i, item in ipairs(items) do
        if item > 0 then
            results[#results + 1] = item * 2
        elseif item == 0 then
            results[#results + 1] = 0
        else
            results[#results + 1] = -item
        end
    end
    return results
end

-- Numeric for loop
for i = 1, 10 do
    -- counting
end

-- While loop
local count = 0
while count < 5 do
    count = count + 1
end

-- Repeat-until loop
repeat
    count = count - 1
until count == 0

-- Coroutine
local co = coroutine.create(function(a, b)
    coroutine.yield(a + b)
    return a * b
end)

-- Multiple return values
local function divmod(a, b)
    return math.floor(a / b), a % b
end

-- Varargs
local function sum(...)
    local total = 0
    for _, v in ipairs({...}) do
        total = total + v
    end
    return total
end

-- Return module
return M
