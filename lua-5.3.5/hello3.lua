function add(x,y)
    return x + y
end

--local ave,sum = average(10,20,30)
local ave,sum = mylib.average(10,20,30)
print("ave : ",ave," sum : ",sum)