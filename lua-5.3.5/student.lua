--local student_obj = Student.create()
--Student.set_name(student_obj,"Jack")
--Student.set_age(student_obj,20)
--print(Student.get_name(student_obj))
--print(Student.get_age(student_obj))
--Student.print(student_obj)



local student_obj = Student.create()
student_obj:set_name("Jack")
student_obj:set_age(20)
print(student_obj:get_name())
print(student_obj:get_age())
student_obj:print()
print(student_obj)
student_obj = nil
collectgarbage("collect")