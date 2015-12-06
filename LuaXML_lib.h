#ifndef LUAXML_LIB_H
#define LUAXML_LIB_H

#ifndef LUAXML_UTF8
# define LUAXML_UTF8	1 /* set to 1 for improved UTF-8 support / robustness */
#endif
#ifndef LUAXML_DEBUG
# define LUAXML_DEBUG	0 /* set to 1 to enable debugging output */
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if defined __WIN32__ || defined WIN32
# include <windows.h>
# define _EXPORT __declspec(dllexport)
#else
# define _EXPORT
#endif

int _EXPORT luaopen_LuaXML_lib (lua_State* L);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LUAXML_LIB_H
