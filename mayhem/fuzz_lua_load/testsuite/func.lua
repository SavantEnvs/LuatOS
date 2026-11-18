local function f(x, ...)
  local t = {x, ...}
  for i, v in ipairs(t) do t[i] = v * 2 end
  return t
end
return f(1, 2, 3)
