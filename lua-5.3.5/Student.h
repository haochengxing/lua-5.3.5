#pragma once

#include <iostream>
#include <string>

class Student
{
public :
	Student();
	~Student();

	std::string get_name();
	void set_name(std::string name);
	unsigned get_age();
	void set_age(unsigned age);

	void print();

private:
	std::string _name;
	unsigned _age;
};

