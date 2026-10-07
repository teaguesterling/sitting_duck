-- line comment
--[[ block comment ]]
local s = "hi"
local t = 'single'
local hex = 0x2a
local d = 3.5

local function greet(name)
  return "hello " .. name
end

print(greet(s), t, hex, d)
