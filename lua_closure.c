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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_exceptions.h"

#include "lua.h"                                                                
#include "lauxlib.h"                                                            
#include "lualib.h"

#include "php_lua.h"

static zend_class_entry *lua_closure_ce;
extern zend_class_entry *lua_ce;
extern zend_class_entry *lua_exception_ce;

/** {{{ ARG_INFO
 *
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_lua_invoke, 0, 0, 1)
	ZEND_ARG_INFO(0, arg)
	ZEND_ARG_INFO(0, ...)
ZEND_END_ARG_INFO()
/* }}} */

/** {{{ zval * php_lua_closure_instance(zval *instance, long ref_id, zval *lua_obj)
 */
zval * php_lua_closure_instance(zval *instance, long ref_id, zval *lua_obj) {
	object_init_ex(instance, lua_closure_ce);
	zend_update_property_long(lua_closure_ce, instance, ZEND_STRL("_closure"), ref_id);
	zend_update_property(lua_closure_ce, instance, ZEND_STRL("_lua_object"), lua_obj);

	return instance;
}
/** }}} */

/** {{{ proto LuaClosure::__construct()
*/
PHP_METHOD(lua_closure, __construct) {
}
/* }}} */

/** {{{ proto LuaClosure::__destruct()
*/
PHP_METHOD(lua_closure, __destruct) {
	zval *lua_obj, *closure, rv;

	lua_obj = zend_read_property(lua_closure_ce, getThis(), ZEND_STRL("_lua_object"), 1, &rv);
	if (ZVAL_IS_NULL(lua_obj)
			|| Z_TYPE_P(lua_obj) != IS_OBJECT
			|| !instanceof_function(Z_OBJCE_P(lua_obj), lua_ce)) {
		RETURN_FALSE;
	}

	closure = zend_read_property(lua_closure_ce, getThis(), ZEND_STRL("_closure"), 1, &rv);
	if (!Z_LVAL_P(closure)) {
		RETURN_FALSE;
	}

	luaL_unref((Z_LUAVAL_P(lua_obj))->L, LUA_REGISTRYINDEX, Z_LVAL_P(closure));
}
/* }}} */

/** {{{ proto LuaClosure::invoke(mxied $args)
*/
PHP_METHOD(lua_closure, invoke) {
	int bp, sp;
	zval *arguments = NULL;
	zval *lua_obj = NULL;
	lua_State *L  = NULL;
	zval *closure = NULL;
	zval rv;

	if (ZEND_NUM_ARGS()) {
		arguments = emalloc(sizeof(zval*) * ZEND_NUM_ARGS());
		if (zend_get_parameters_array_ex(ZEND_NUM_ARGS(), arguments) == FAILURE) {
			efree(arguments);
			zend_throw_exception_ex(NULL, 0, "cannot get arguments for calling closure");
			return;
		}
	}

	lua_obj = zend_read_property(lua_closure_ce, getThis(), ZEND_STRL("_lua_object"), 1, &rv);

	if (ZVAL_IS_NULL(lua_obj)
			|| Z_TYPE_P(lua_obj) != IS_OBJECT
			|| !instanceof_function(Z_OBJCE_P(lua_obj), lua_ce)) {
		zend_throw_exception_ex(NULL, 0, "corrupted Lua object");
		return;
	}

	closure = zend_read_property(lua_closure_ce, getThis(), ZEND_STRL("_closure"), 1, &rv);
	if (!Z_LVAL_P(closure)) {
		zend_throw_exception_ex(NULL, 0, "invalid lua closure");
		return;
	}

	L = (Z_LUAVAL_P(lua_obj))->L;

	bp = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, Z_LVAL_P(closure));
	if (LUA_TFUNCTION != lua_type(L, lua_gettop(L))) {
		lua_pop(L, -1);
		zend_throw_exception_ex(NULL, 0, "call to lua closure failed");
		return;
	}

	if (ZEND_NUM_ARGS()) {
		int i = 0;
		for(;i<ZEND_NUM_ARGS();i++) {
			php_lua_send_zval_to_lua(L, &arguments[i]);
		}
	}

	if (lua_pcall(L, ZEND_NUM_ARGS(), LUA_MULTRET, 0) != 0) {
		if (arguments) {
			efree(arguments);
		}
		lua_pop(L, lua_gettop(L) - bp);
		zend_throw_exception_ex(NULL, 0, 
				"call to lua function %s failed", lua_tostring(L, -1));
		return;
	}

	sp = lua_gettop(L) - bp;

	if (!sp) {
		RETURN_NULL();
	} else if (sp == 1) {
		php_lua_get_zval_from_lua(L, -1, lua_obj, return_value);
	} else {
		zval rv;
		int  i = 0;
		array_init(return_value);
		for (i = -sp; i < 0; i++) {
			php_lua_get_zval_from_lua(L, i, lua_obj, &rv);
			add_next_index_zval(return_value, &rv);
		}
	}

	lua_pop(L, sp);

	if (arguments) {
		efree(arguments);
	}
}
/* }}} */

/** {{{ proto LuaClosure::__clone()
*/
PHP_METHOD(lua_closure, __clone) {
}
/* }}} */

/* {{{ lua_class_methods[]
 *
 */
zend_function_entry lua_closure_methods[] = {
	PHP_ME(lua_closure, __construct,		NULL,  					ZEND_ACC_PRIVATE|ZEND_ACC_CTOR)
	PHP_ME(lua_closure, __destruct,			NULL,  					ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(lua_closure, __clone,			NULL,  					ZEND_ACC_PRIVATE)
	PHP_ME(lua_closure, invoke,				arginfo_lua_invoke,  	ZEND_ACC_PUBLIC)
	PHP_MALIAS(lua_closure, __invoke, invoke, arginfo_lua_invoke,	ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

static void php_lua_closure_dtor_object(void *object, zend_object_handlers handle) /* {{{ */
{
	zend_object *obj = (zend_object*)object;

	zend_object_std_dtor(obj);

	efree(obj);
} /* }}} */

zend_object *php_lua_closure_create_object(zend_class_entry *ce) /* {{{ */
{
	zend_object *intern;
	
	intern = emalloc(sizeof(zend_object)+ sizeof(zval) * (ce->default_properties_count - 1));

	if (!intern) {
		php_error_docref(NULL, E_ERROR, "alloc memory for lua object failed");
	}

	zend_object_std_init(intern, ce);
	object_properties_init(intern, ce);

	intern->handlers = zend_get_std_object_handlers();
	
	return intern;
	
} /* }}} */

void php_lua_closure_register() /* {{{ */
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "LuaClosure", lua_closure_methods);
	lua_closure_ce = zend_register_internal_class(&ce);
	lua_closure_ce->create_object = php_lua_closure_create_object;
	lua_closure_ce->ce_flags |= ZEND_ACC_FINAL;

	zend_declare_property_long(lua_closure_ce, ZEND_STRL("_closure"), 0, ZEND_ACC_PRIVATE);
	zend_declare_property_null(lua_closure_ce, ZEND_STRL("_lua_object"), ZEND_ACC_PRIVATE);


} /* }}} */

zend_class_entry *php_lua_get_closure_ce() /* {{{ */
{
	return lua_closure_ce;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
