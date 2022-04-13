--study3
global_c_read_val = "test val"
print(global_c_write_val)
--study4
global_c_read_table = {integer_val = 1,double_val = 2.34,string_val = "test_string"}
for k,v in pairs(global_c_write_table) do 
    print("k = ",k," v = ",v)
end