local weakTbl = {
    "dasdasd",
    {dd = "dddd"},
}
setmetatable(weakTbl,{
    __mode = "v",
})
 
-- local dan = weakTbl[2]  -- 此处为强引用  执行后weakTbl[2]将不会被回收
collectgarbage()
 
for k,v in pairs(weakTbl) do
    print(k, v) 
end

local tbl = {}
local mt = {__mode = "k"}
setmetatable(tbl, mt)
 
local key = {count = 100}
tbl[key] = 2
 
-- key = {}  -- 此处重新赋值 导致原key存储的地址  现在只有弱表tbl引用
 
-- 强制垃圾收集
collectgarbage()
 
print("log print: ")
for k,v in pairs(tbl) do
    print(k, v, k.count) -- table: 00000000001f9730 2 nil
end

t = {};

-- 给t设置一个元表，增加__mode元方法，赋值为“k”
setmetatable(t, {__mode = "k"});
   
-- 使用一个table作为t的key值
key1 = {name = "key1"};
t[key1] = 1;
key1 = nil;
-- 又使用一个table作为t的key值
key2 = {name = "key2"};
t[key2] = 1;
key2 = nil;
   
-- 强制进行一次垃圾收集
collectgarbage();
   
for key, value in pairs(t) do
    print(key.name .. ":" .. value);
end

local t = {}
local key2 = { name = 'key2'}
table.insert(t, key2)
key2 = nil
 
setmetatable(t, {__mode = "kv"})   --设置 table，key 和 value 的弱引用
collectgarbage()                  --手动触发垃圾回收
 
for k,v in pairs(t) do
	print(k,v)
	for k1,v1 in pairs(v) do
		print(k1,v1)
	end
end