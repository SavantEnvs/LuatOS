local mt = {__index = function(t, k) return k end, __add = function(a, b) return 0 end}
local o = setmetatable({}, mt)
local c <const> = 5
local d <close> = nil
return o.x, o + o, c, 3 & 5 | 1 ~ 2 << 1, #'abc', not nil
