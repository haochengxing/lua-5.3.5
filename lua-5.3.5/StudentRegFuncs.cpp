#include "StudentRegFuncs.h"

/*

�ڴ� | �ڴ��ַ * | ���ڴ��ַ�ĵ�ַ ** (���ʻ���ָ��)

��ջ�з�����һ�� ��ָ���ָ��
ʵ������4�ֽڣ�ָ���С����һ���ڴ�
ת�����ܹ��洢Student* ��һ��ָ��

* s ���õ���һ�� ����ָ��
new���� �����ڴ��ַ ��  ��ֵ������ָ��

*/
int lua_create_new_student(lua_State* L)
{
	//�����������һ��ָ����С���ڴ�飬 ���ڴ���ַ��Ϊһ����ȫ�û�����ѹջ�� �����������ַ�� ���������������ʹ������ڴ档
	Student** s = (Student**)lua_newuserdata(L,sizeof(Student*));
	*s = new Student();

	//�����ȫ�ֱ����ʹ��LUA_REGISTRYINDEX������Lua�еõ���
	//lua->stack
	//lua_getfield(L, LUA_REGISTRYINDEX, "StudentClass");

	////-------�ȼ���������������------
	//lua_pushstring("StudentClass");
	//lua_gettable(L, LUA_REGISTRYINDEX);

	//Lua->Stack���õ�ȫ��Ԫ��λ��-1��userdataλ��-2
	luaL_getmetatable(L,"StudentClass");

	//��Ԫ��ֵ��λ��-2��userdata��������-1��Ԫ��
	lua_setmetatable(L,-2);

	return 1;
}

int lua_get_name(lua_State* L)
{
	//�õ���һ������Ķ������(��stack��ײ�)
	Student** s = (Student**)luaL_checkudata(L,1,"StudentClass");
	//Student** s = (Student**)lua_touserdata(L, 1);
	//���cond������Ϊtrue�����׳�һ�����󱨸���õ� C �����ĵ� arg �����������⡣
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	lua_settop(L,0);
	lua_pushstring(L, (*s)->get_name().c_str());
	return 1;
}

int lua_set_name(lua_State* L)
{
	//�õ���һ������Ķ������(��stack��ײ�)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	//Student** s = (Student**)lua_touserdata(L,1);
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	//��麯���ĵ� arg �������������Ƿ��� t��
	luaL_checktype(L, -1, LUA_TSTRING);
	std::string name = lua_tostring(L, -1);
	(*s)->set_name(name);
	return 1;
}

int lua_get_age(lua_State* L)
{
	//�õ���һ������Ķ������(��stack��ײ�)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	//Student** s = (Student**)lua_touserdata(L,1);
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	lua_pushinteger(L, (*s)->get_age());
	return 1;
}

int lua_set_age(lua_State* L)
{
	//�õ���һ������Ķ������(��stack��ײ�)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	//Student** s = (Student**)lua_touserdata(L,1);
	luaL_argcheck(L,s!=NULL,1,"invalid user data");
	luaL_checktype(L, -1, LUA_TNUMBER);
	(*s)->set_age((unsigned)lua_tointeger(L,-1));
	return 1;
}

int lua_print(lua_State* L)
{
	//�õ���һ������Ķ������(��stack��ײ�)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	//Student** s = (Student**)lua_touserdata(L,1);
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	(*s)->print();
	return 1;
}

int lua_student2string(lua_State* L)
{
	//�õ���һ������Ķ������(��stack��ײ�)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	lua_pushfstring(L, "This is student name : %s age : %d !", (*s)->get_name().c_str(), (*s)->get_age());
	return 1;
}

int lua_auto_gc(lua_State* L)
{
	//�õ���һ������Ķ������(��stack��ײ�)
	Student** s = (Student**)luaL_checkudata(L, 1, "StudentClass");
	luaL_argcheck(L, s != NULL, 1, "invalid user data");
	if (s) {
		delete* s;

	}
	return 1;
}

int luaopen_student_libs(lua_State* L)
{
	//�ڿ���ʹ����Ӧ��lua_setfield��������table,�����-1ʹ��LUA_REGISTRYINDEX����������ȫ�ֱ���KeyΪ"StudentClass"��ֵ������Ĵ�����ǽ�Ԫ����Ϊֵ��
	//lua_pushinteger(L, 66); //val
	//lua_setfield(L, -1, "StudentClass");

	////-------�ȼ������溯��------
	//lua_pushstring("StudentClass"); //key
	//lua_pushinteger(L, 66); //val
	//lua_settable(L, -1);

	//����ȫ��Ԫ��(��������˶�LUA_REGISTRYINDEX�Ĳ���)��Ԫ���λ��Ϊ-1
	luaL_newmetatable(L,"StudentClass");

	//��Ԫ����Ϊһ������ѹջ��λ��-1��ԭԪ��λ��-2
	lua_pushvalue(L,-1);

	//����-2λ��Ԫ���__index������ֵΪλ��-1��Ԫ��������λ��-1��Ԫ��ԭԪ���λ��Ϊ-1
	lua_setfield(L, -2, "__index");

	//����Ա����ע�ᵽԪ����(�����ȫ��Ԫ������þ�ȫ�������)
	luaL_setfuncs(L, lua_reg_student_member_funcs, 0);

	//ע�ṹ�캯�����±��У������ظ�Lua
	luaL_newlib(L,lua_reg_student_constructor_funcs);


	//luaL_newlib(L, lua_reg_student_funcs);
	return 1;
}
