/*
  +----------------------------------------------------------------------+
  | Lua                                                                  |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author    : Johannes Schlueter  <johannes@php.net>                   |
  |             Xinchen  Hui        <laruence@php.net>                   |
  |             Marcelo  Araujo     <msaraujo@php.net>                   |
  |             Helmut Januschka    <helmut@januschka.com>               |
  +----------------------------------------------------------------------+
*/

#ifndef PHP_LUA_H
#define PHP_LUA_H

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/* LUA_OK is defined sinc 5.2 */
#ifndef LUA_OK
#define LUA_OK 0
#endif

extern zend_module_entry lua_module_entry;
#define phpext_lua_ptr &lua_module_entry

#ifdef PHP_WIN32
#define PHP_LUA_API __declspec(dllexport)
#else
#define PHP_LUA_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#ifdef ZTS
#define LUA_G(v) TSRMG(lua_globals_id, zend_lua_globals *, v)
#else
#define LUA_G(v) (lua_globals.v)
#endif

#define PHP_LUA_VERSION "2.0.7"

struct _php_lua_object {
  lua_State *L;
  zend_object obj;
};

typedef struct _php_lua_object php_lua_object;

static inline php_lua_object *php_lua_obj_from_obj(zend_object *obj) {
  return (php_lua_object*)((char*)(obj)-XtOffsetOf(php_lua_object, obj));
}

#define Z_LUAVAL(obj)   php_lua_obj_from_obj(Z_OBJ((obj)))
#define Z_LUAVAL_P(obj) php_lua_obj_from_obj(Z_OBJ_P((obj)))

zval *php_lua_get_zval_from_lua(lua_State *L, int index, zval *lua_obj, zval *rv);
int php_lua_send_zval_to_lua(lua_State *L, zval *val);

PHP_MINIT_FUNCTION(lua);
PHP_MSHUTDOWN_FUNCTION(lua);
PHP_MINFO_FUNCTION(lua);

PHP_METHOD(lua, __construct);
PHP_METHOD(lua, eval);
PHP_METHOD(lua, require);

#endif	/* PHP_LUA_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
