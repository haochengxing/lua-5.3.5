#pragma once

#include "Student.h"

extern "C" {
#include "src/lua.h"
#include "src/lauxlib.h"
}

int lua_create_new_student(lua_State* L);

int lua_get_name(lua_State* L);
int lua_set_name(lua_State* L);
int lua_get_age(lua_State* L);
int lua_set_age(lua_State* L);

int lua_print(lua_State* L);

int lua_student2string(lua_State* L);

int lua_auto_gc(lua_State* L);

static const luaL_Reg lua_reg_student_constructor_funcs[] = {
	{"create",lua_create_new_student},
	{NULL,NULL},
};

static const luaL_Reg lua_reg_student_member_funcs[] = {
	{"get_name",lua_get_name},
	{"set_name",lua_set_name},
	{"get_age",lua_get_age},
	{"set_age",lua_set_age},
	{"print",lua_print},
	{"__gc",lua_auto_gc},
	{"__tostring",lua_student2string},
	{NULL,NULL},
};


/*static const luaL_Reg lua_reg_student_funcs[] = {
	{"create",lua_create_new_student},
	{"get_name",lua_get_name},
	{"set_name",lua_set_name},
	{"get_age",lua_get_age},
	{"set_age",lua_set_age},
	{"print",lua_print},
	{NULL,NULL},
};*/

int luaopen_student_libs(lua_State* L);