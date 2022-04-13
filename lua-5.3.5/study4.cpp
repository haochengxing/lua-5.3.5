#include <iostream>
#include <string.h>
using namespace std;

extern "C" {
#include "src/lua.h"
#include "src/lauxlib.h"
#include "src/lualib.h"
}

void main_4() {
	lua_State *L = luaL_newstate();
	if (L==NULL)
	{
		return;
	}

	luaL_openlibs(L);

	int bRet = luaL_loadfile(L, "hello1.lua");
	if (bRet) {
		return;
	}

	
	//c->stack������һ���µ�table�ŵ�-1��λ��
	lua_newtable(L);

	//c->stack,���һ��key���ŵ�-1��λ�ã�tableλ�ñ�Ϊ-2
	lua_pushstring(L, "integer_val");

	//c->stack������key��Ӧ��ֵ���ŵ�-1λ�ã�keyλ�ñ�Ϊ-2��tableλ�ñ�Ϊ-3
	lua_pushinteger(L,1);

	//��key��val���õ�table�У�������key��val����ʱtableλ�ñ�Ϊ-1
	lua_settable(L,-3);

	//stack->lua����table��ֵ��lua��������table
	lua_setglobal(L, "global_c_write_table");

	bRet = lua_pcall(L,0,0,0);
	if (bRet) {
		return;
	}

	//lua->stack,�õ�ȫ�ֱ�λ��-1
	lua_getglobal(L, "global_c_read_table");

	//�õ���һ��ֵ
	//c->stack,����keyֵ,λ��-1(�����-1��Ϊ-2)
	lua_pushstring(L, "integer_val");

	//lua->stack,��-1λ�õ�keyֵ����,��lua�еõ���Ӧkey��ֵ�������������-1��λ��
	//���û��ֵ������ΪTNIL
	lua_gettable(L, -2);

	//stack->c
	if (lua_isinteger(L, -1)) {
		cout << "integer_val:" << lua_tointeger(L, -1) << endl;
	}

	//����-1λ�õĽ����֮��ȫ�ֱ��λ�ûָ���-1
	lua_pop(L, 1);

	//�ظ��������裬�õ��ڶ���ֵ
	lua_pushstring(L, "double_val");
	lua_gettable(L, -2);//����ֵΪֵ������
	if (lua_isnumber(L, -1)) {
		cout << "double_val:" << lua_tonumber(L, -1) << endl;
	}
	lua_pop(L, 1);

	//�ظ��������裬�õ�������ֵ
	lua_pushstring(L, "string_val");
	lua_gettable(L, -2);//����ֵΪֵ������
	if (lua_isstring(L, -1)) {
		cout << "string_val:" << lua_tostring(L, -1) << endl;
	}
	lua_pop(L, 1);

	//ʹ��lua_getfield����lua_pushxxx��lua_gettable��������������ֵΪֵ������
	lua_getfield(L, -1, "integer_val");
	cout << "integer_val:" << lua_tointeger(L, -1) << endl;

	lua_close(L);

	return;
}