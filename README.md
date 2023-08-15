# lua-5.3.5

转载两篇讲lua源码的CSDN文章

https://blog.csdn.net/initphp/category_9293184.html

https://blog.csdn.net/liutianshx2012/category_2775545.html

转载C++和lua交互是如何使用堆栈的方式

https://blog.csdn.net/m1234567q/article/details/114888047

转载Lua中以面向对象的方式使用C++注册的类

https://www.cnblogs.com/chevin/p/5897220.html

转载分析lua的GC

https://zhuanlan.zhihu.com/p/133939450

https://zhuanlan.zhihu.com/p/393150733

Unity中xLua与toLua对Vector3的优化

https://it.cha138.com/jingpin/show-194524.html


```lua
local v = CS.UnityEngine.Vector3(7, 8, 9)
```

```csharp
static int __CreateInstance(RealStatePtr L)
{
	var gen_ret = new UnityEngine.Vector3(_x, _y, _z);
	translator.PushUnityEngineVector3(L, gen_ret);
}

public void PushUnityEngineVector3(RealStatePtr L, UnityEngine.Vector3 val)
{
	IntPtr buff = LuaAPI.xlua_pushstruct(L, 12, UnityEngineVector3_TypeID);
	
    LuaAPI.xlua_pack_float3(buff, offset, field.x, field.y, field.z))
}

public void Get(RealStatePtr L, int index, out UnityEngine.Vector3 val)
{
	LuaTypes type = LuaAPI.lua_type(L, index);
    if (type == LuaTypes.LUA_TUSERDATA )
    {
		CopyByValue.UnPack(buff, 0, out val))
    }
	else if (type ==LuaTypes.LUA_TTABLE)
	{
		CopyByValue.UnPack(this, L, index, out val);
	}
    else
    {
        val = (UnityEngine.Vector3)objectCasters.GetCaster(typeof(UnityEngine.Vector3))(L, index, null);
    }
}

public static bool UnPack(IntPtr buff, int offset, out UnityEngine.Vector3 field)
{
    field = default(UnityEngine.Vector3);
    
    float x = default(float);
    float y = default(float);
    float z = default(float);
    
    if(!LuaAPI.xlua_unpack_float3(buff, offset, out x, out y, out z))
    {
        return false;
    }
    field.x = x;
    field.y = y;
    field.z = z;
        
    return true;
}


public static void UnPack(ObjectTranslator translator, RealStatePtr L, int idx, out UnityEngine.Vector3 val)
{
    val = new UnityEngine.Vector3();
    int top = LuaAPI.lua_gettop(L);
    
    if (Utils.LoadField(L, idx, "x"))
    {
        
        translator.Get(L, top + 1, out val.x);
        
    }
    LuaAPI.lua_pop(L, 1);
    
    if (Utils.LoadField(L, idx, "y"))
    {
        
        translator.Get(L, top + 1, out val.y);
        
    }
    LuaAPI.lua_pop(L, 1);
    
    if (Utils.LoadField(L, idx, "z"))
    {
        
        translator.Get(L, top + 1, out val.z);
        
    }
    LuaAPI.lua_pop(L, 1);
    
}
```

```csharp
[GCOptimize]
static List<Type> GCOptimize
{
    get
    {
        return new List<Type>() {
            typeof(UnityEngine.Vector2),
            typeof(UnityEngine.Vector3),
            typeof(UnityEngine.Vector4),
            typeof(UnityEngine.Color),
            typeof(UnityEngine.Quaternion),
            typeof(UnityEngine.Ray),
            typeof(UnityEngine.Bounds),
            typeof(UnityEngine.Ray2D),
        };
    }
}
```