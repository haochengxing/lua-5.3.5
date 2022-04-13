#include "Student.h"

Student::Student() :_name("Empty"), _age(0) {
	std::cout << "Student Constructor" << std::endl;
}

Student::~Student() {
	std::cout << "Student Destructor" << std::endl;
}

std::string Student::get_name()
{
	return _name;
}

void Student::set_name(std::string name)
{
	_name = name;
}

unsigned Student::get_age()
{
	return _age;
}

void Student::set_age(unsigned age)
{
	_age = age;
}

void Student::print()
{
	std::cout << "name:" << _name << " age:" << _age << std::endl;
}

