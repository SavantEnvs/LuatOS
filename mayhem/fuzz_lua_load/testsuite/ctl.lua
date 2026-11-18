local s = 'a\x41\u{48}'
local n = 0
while n < 10 do n = n + 1 if n == 5 then goto done end end
::done::
repeat n = n - 1 until n <= 0
return s .. [[long]] .. 0x1p4
