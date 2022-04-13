#include "StudentRegFuncs.h"

/*

内存 | 内存地址 * | 存内存地址的地址 ** (本质还是指针)

从栈中分配了一个 存指针的指针
实际上是4字节（指针大小）的一段内存
转化成能够存储Student* 的一个指针

* s 就拿到了一个 对象指针
new对象 返回内存地址 ，  赋值给对象指针

*/
int lua_create_new_student(lua_State* L)
{
	//这个函数分配一块指定大小的内存块， 把内存块地址作为一个完全用户数据压栈， 并返回这个地址。 宿主程序可以随意使用这块内存。
	Student** s = (Student**)lua_newuserdata(L,sizeof(Student*));
	*s = new Student();

	//①这个全局表可以使用LUA_REGISTRYINDEX索引从Lua中得到。
	//lua->stack
	//lua_getfield(L, LUA_REGISTRYINDEX, "StudentClass");

	////-------等价于下面两个函数------
	//lua_pushstring("StudentClass");
	//lua_gettable(L, LUA_REGISTRYINDEX);

	//Lua->Stack，得到全局元表位置-1，userdata位置-2
	luaL_getmetatable(L,"StudentClass");

	//将元表赋值给位置-2的userdata，并弹出-1的元表
	lua_setmetatable(L,-2);

	return 1;
}

int lua_get_name(lua_State* L)
{
	//得到第一个传入的对象参数(在stack最底部)
	Student** s = (Student**)luaL_checkudata(L,1,"StudentClass");
	//Student** s = (Student**)lua_touserdata(L, 1);
	//如果cond条件不为true，则抛出一个错误报告调用的 C 函数的第 arg 个参数的问题。
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	lua_settop(L,0);
	lua_pushstring(L, (*s)->get_name().c_str());
	return 1;
}

int lua_set_name(lua_State* L)
{
	//得到第一个传入的对象参数(在stack最底部)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	//Student** s = (Student**)lua_touserdata(L,1);
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	//检查函数的第 arg 个参数的类型是否是 t。
	luaL_checktype(L, -1, LUA_TSTRING);
	std::string name = lua_tostring(L, -1);
	(*s)->set_name(name);
	return 1;
}

int lua_get_age(lua_State* L)
{
	//得到第一个传入的对象参数(在stack最底部)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	//Student** s = (Student**)lua_touserdata(L,1);
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	lua_pushinteger(L, (*s)->get_age());
	return 1;
}

int lua_set_age(lua_State* L)
{
	//得到第一个传入的对象参数(在stack最底部)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	//Student** s = (Student**)lua_touserdata(L,1);
	luaL_argcheck(L,s!=NULL,1,"invalid user data");
	luaL_checktype(L, -1, LUA_TNUMBER);
	(*s)->set_age((unsigned)lua_tointeger(L,-1));
	return 1;
}

int lua_print(lua_State* L)
{
	//得到第一个传入的对象参数(在stack最底部)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	//Student** s = (Student**)lua_touserdata(L,1);
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	(*s)->print();
	return 1;
}

int lua_student2string(lua_State* L)
{
	//得到第一个传入的对象参数(在stack最底部)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	lua_pushfstring(L, "This is student name : %s age : %d !", (*s)->get_name().c_str(), (*s)->get_age());
	return 1;
}

int lua_auto_gc(lua_State* L)
{
	//得到第一个传入的对象参数(在stack最底部)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	if (s) {
		delete* s;

	}
	return 1;
}

int luaopen_student_libs(lua_State* L)
{
	//②可以使用相应的lua_setfield函数设置table,下面的-1使用LUA_REGISTRYINDEX，就是设置全局表中Key为"StudentClass"的值（后面的代码就是将元表作为值）
	//lua_pushinteger(L, 66); //val
	//lua_setfield(L, -1, "StudentClass");

	////-------等价于下面函数------
	//lua_pushstring("StudentClass"); //key
	//lua_pushinteger(L, 66); //val
	//lua_settable(L, -1);

	//创建全局元表(里面包含了对LUA_REGISTRYINDEX的操作)，元表的位置为-1
	luaL_newmetatable(L,"StudentClass");

	//将元表作为一个副本压栈到位置-1，原元表位置-2
	lua_pushvalue(L,-1);

	//设置-2位置元表的__index索引的值为位置-1的元表，并弹出位置-1的元表，原元表的位置为-1
	lua_setfield(L, -2, "__index");

	//将成员函数注册到元表中(到这里，全局元表的设置就全部完成了)
	luaL_setfuncs(L, lua_reg_student_member_funcs, 0);

	//注册构造函数到新表中，并返回给Lua
	luaL_newlib(L,lua_reg_student_constructor_funcs);


	//luaL_newlib(L, lua_reg_student_funcs);
	return 1;
}
