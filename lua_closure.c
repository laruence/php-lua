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
  | 			Xinchen  Hui        <laruence@php.net>                   |
  |             Marcelo  Araujo     <msaraujo@php.net>                   |
  +----------------------------------------------------------------------+
   $Id: lua_closure.c 319740 2011-11-24 08:06:48Z laruence $
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

/** {{{ zval * php_lua_closure_instance(zval *instance, long ref_id, zval *lua_obj TSRMLS_DC)
 */
zval * php_lua_closure_instance(zval *instance, long ref_id, zval *lua_obj TSRMLS_DC) {
	object_init_ex(instance, lua_closure_ce);
	zend_update_property_long(lua_closure_ce, instance, ZEND_STRL("_closure"), ref_id TSRMLS_CC);
	zend_update_property(lua_closure_ce, instance, ZEND_STRL("_lua_object"), lua_obj TSRMLS_CC);

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
	zval *lua_obj, *closure;

	lua_obj = zend_read_property(lua_closure_ce, getThis(), ZEND_STRL("_lua_object"), 1 TSRMLS_CC);
	if (ZVAL_IS_NULL(lua_obj)
			|| Z_TYPE_P(lua_obj) != IS_OBJECT
			|| !instanceof_function(Z_OBJCE_P(lua_obj), lua_ce TSRMLS_CC)) {
		RETURN_FALSE;
	}

	closure = zend_read_property(lua_closure_ce, getThis(), ZEND_STRL("_closure"), 1 TSRMLS_CC);
	if (!Z_LVAL_P(closure)) {
		RETURN_FALSE;
	}

	luaL_unref(Z_LUAVAL_P(lua_obj), LUA_REGISTRYINDEX, Z_LVAL_P(closure));
}
/* }}} */

/** {{{ proto LuaClosure::invoke(mxied $args)
*/
PHP_METHOD(lua_closure, invoke) {
	int bp, sp;
	zval ***arguments = NULL;
	zval *lua_obj = NULL;
	lua_State *L  = NULL;
	zval *closure = NULL;

	if (ZEND_NUM_ARGS()) {
		arguments = emalloc(sizeof(zval**) * ZEND_NUM_ARGS());
		if (zend_get_parameters_array_ex(ZEND_NUM_ARGS(), arguments) == FAILURE) {
			efree(arguments);
			zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "cannot get arguments for calling closure");
			return;
		}
	}

	lua_obj = zend_read_property(lua_closure_ce, getThis(), ZEND_STRL("_lua_object"), 1 TSRMLS_CC);

	if (ZVAL_IS_NULL(lua_obj)
			|| Z_TYPE_P(lua_obj) != IS_OBJECT
			|| !instanceof_function(Z_OBJCE_P(lua_obj), lua_ce TSRMLS_CC)) {
		zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "corrupted Lua object");
		return;
	}

	closure = zend_read_property(lua_closure_ce, getThis(), ZEND_STRL("_closure"), 1 TSRMLS_CC);
	if (!Z_LVAL_P(closure)) {
		zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "invalid lua closure");
		return;
	}

	L = Z_LUAVAL_P(lua_obj);

	bp = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, Z_LVAL_P(closure));
	if (LUA_TFUNCTION != lua_type(L, lua_gettop(L))) {
		lua_pop(L, -1);
		zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "call to lua closure failed");
		return;
	}

	if (ZEND_NUM_ARGS()) {
		int i = 0;
		for(;i<ZEND_NUM_ARGS();i++) {
			php_lua_send_zval_to_lua(L, *(arguments[i]) TSRMLS_CC);
		}
	}

	if (lua_pcall(L, ZEND_NUM_ARGS(), LUA_MULTRET, 0) != 0) {
		if (arguments) {
			efree(arguments);
		}
		lua_pop(L, lua_gettop(L) - bp);
		zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, 
				"call to lua function %s failed", lua_tostring(L, -1));
		return;
	}

	sp = lua_gettop(L) - bp;

	if (!sp) {
		RETURN_NULL();
	} else if (sp == 1) {
		zval *tmp = php_lua_get_zval_from_lua(L, -1, lua_obj TSRMLS_CC);
		RETURN_ZVAL(tmp, 0, 0);
	} else {
		int  i = 0;
		array_init(return_value);
		for (i = -sp; i < 0; i++) {
			zval *tmp = php_lua_get_zval_from_lua(L, i, lua_obj TSRMLS_CC);
			add_next_index_zval(return_value, tmp);
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

static void php_lua_closure_dtor_object(void *object, zend_object_handle handle TSRMLS_DC) /* {{{ */
{
	zend_object *obj = (zend_object*)object;

	zend_object_std_dtor(obj TSRMLS_CC);

	efree(obj);
} /* }}} */

static zend_object_value php_lua_closure_create_object(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	zend_object_value obj    = {0};
	zend_object *lua_closure_obj = NULL;

	lua_closure_obj = emalloc(sizeof(zend_object));

	zend_object_std_init(lua_closure_obj, ce TSRMLS_CC);
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)
	zend_hash_copy(lua_closure_obj->properties, &ce->default_properties, 
#if (PHP_MINOR_VERSION < 3) 
			(copy_ctor_func_t) zval_add_ref,
#else
			zval_copy_property_ctor(ce), 
#endif
			(void *)0, sizeof(zval *));
#elif (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION > 3)
	object_properties_init(lua_closure_obj, ce);	
#endif

	obj.handle   = zend_objects_store_put(lua_closure_obj, php_lua_closure_dtor_object, NULL, NULL TSRMLS_CC);
	obj.handlers = zend_get_std_object_handlers();

	return obj;
} /* }}} */

void php_lua_closure_register(TSRMLS_D) /* {{{ */
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "LuaClosure", lua_closure_methods);
	lua_closure_ce = zend_register_internal_class(&ce TSRMLS_CC);
	lua_closure_ce->create_object = php_lua_closure_create_object;
	lua_closure_ce->ce_flags |= ZEND_ACC_FINAL;

	zend_declare_property_long(lua_closure_ce, ZEND_STRL("_closure"), 0, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(lua_closure_ce, ZEND_STRL("_lua_object"), ZEND_ACC_PRIVATE TSRMLS_CC);


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
