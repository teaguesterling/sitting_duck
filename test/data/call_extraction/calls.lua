local function helper(x)
    return x + 1
end

local obj = {}
function obj.render(s)
    return helper(#s)
end

helper(1)
obj:render("a")
