-- #220 fixture: every parameter shape lua has, plus the two forms that must
-- deliver ZERO so a legitimate zero is not read as the old silent failure.
local function greet(who)
  return "hi " .. who
end

function M.create(name, value, ...)
  return { name = name, value = value }
end

local anon = function(a, b)
  return a + b
end

function noargs()
  return 1
end

function Animal:speak()
  return "..."
end
