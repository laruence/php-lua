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
  |             Helmut  Januschka   <helmut@januschka.com>               |
  +----------------------------------------------------------------------+
*/

#ifndef LUA_CLOSURE_H
#define LUA_CLOSURE_H
typedef struct _lua_closure_object {
	long closure;
	zval lua;

	zend_object std;
} lua_closure_object;

static inline lua_closure_object* php_lua_closure_object_from_zend_object(zend_object *zobj) {
	return ((lua_closure_object*)(zobj + 1)) - 1;
}

void php_lua_closure_register();
zend_class_entry *php_lua_get_closure_ce();
zval * php_lua_closure_instance(zval *instance, long ref_id, zval *lua_obj);
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
