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
#include "lua_closure.h"
#include "lauxlib.h"                                                            
#include "lualib.h"

#include "php_lua.h"

static zend_class_entry *lua_closure_ce;
extern zend_class_entry *lua_ce;
extern zend_class_entry *lua_exception_ce;
static zend_object_handlers lua_closure_handlers;

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
zval* php_lua_closure_instance(zval *instance, long ref_id, zval *lua_obj) {
	lua_closure_object *objval;

	object_init_ex(instance, lua_closure_ce);
	objval = php_lua_closure_object_from_zend_object(Z_OBJ_P(instance));
	objval->closure = ref_id;
	if (lua_obj) {
		ZVAL_ZVAL(&(objval->lua), lua_obj, 1, 0);
	}

	return instance;
}
/** }}} */

/** {{{ proto LuaClosure::__construct()
*/
PHP_METHOD(lua_closure, __construct) {
}
/* }}} */

/** {{{ proto LuaClosure::invoke(mxied $args)
*/
PHP_METHOD(lua_closure, invoke) {
	lua_closure_object *objval = php_lua_closure_object_from_zend_object(Z_OBJ_P(getThis()));
	int bp, sp;
	zval *arguments = NULL;
	lua_State *L  = NULL;
	zval rv;

	if (ZEND_NUM_ARGS()) {
		arguments = emalloc(sizeof(zval) * ZEND_NUM_ARGS());
		if (zend_get_parameters_array_ex(ZEND_NUM_ARGS(), arguments) == FAILURE) {
			efree(arguments);
			zend_throw_exception_ex(NULL, 0, "cannot get arguments for calling closure");
			return;
		}
	}

	if (Z_TYPE(objval->lua) != IS_OBJECT
			|| !instanceof_function(Z_OBJCE(objval->lua), lua_ce)) {
		zend_throw_exception_ex(NULL, 0, "corrupted Lua object");
		return;
	}

	L = (Z_LUAVAL(objval->lua))->L;

	bp = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, objval->closure);
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
		php_lua_get_zval_from_lua(L, -1, &(objval->lua), return_value);
	} else {
		zval rv;
		int  i = 0;
		array_init(return_value);
		for (i = -sp; i < 0; i++) {
			php_lua_get_zval_from_lua(L, i, &(objval->lua), &rv);
			add_next_index_zval(return_value, &rv);
		}
	}

	lua_pop(L, sp);

	if (arguments) {
		efree(arguments);
	}
}
/* }}} */

/* {{{ lua_class_methods[]
 */
zend_function_entry lua_closure_methods[] = {
	PHP_ME(lua_closure, __construct,		NULL,  					ZEND_ACC_PRIVATE|ZEND_ACC_CTOR)
	PHP_ME(lua_closure, invoke,				arginfo_lua_invoke,  	ZEND_ACC_PUBLIC)
	PHP_MALIAS(lua_closure, __invoke, invoke, arginfo_lua_invoke,	ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

static void php_lua_closure_free_obj(zend_object *zobj) /* {{{ */
{
	lua_closure_object *objval = php_lua_closure_object_from_zend_object(zobj);

	if ((Z_TYPE(objval->lua) == IS_OBJECT) &&
		instanceof_function(Z_OBJCE(objval->lua), lua_ce)) {
		luaL_unref((Z_LUAVAL(objval->lua))->L, LUA_REGISTRYINDEX, objval->closure);
	}
	zval_dtor(&(objval->lua));
	zend_object_std_dtor(zobj);
} /* }}} */

zend_object *php_lua_closure_create_object(zend_class_entry *ce) /* {{{ */
{
	lua_closure_object *objval = ecalloc(1, sizeof(lua_closure_object) + zend_object_properties_size(ce));
	zend_object *zobj = &(objval->std);

	zend_object_std_init(zobj, ce);
	zobj->handlers = &lua_closure_handlers;
	
	return zobj;
} /* }}} */

void php_lua_closure_register() /* {{{ */
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "LuaClosure", lua_closure_methods);
	lua_closure_ce = zend_register_internal_class(&ce);
	lua_closure_ce->create_object = php_lua_closure_create_object;
	lua_closure_ce->ce_flags |= ZEND_ACC_FINAL;

	memcpy(&lua_closure_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	lua_closure_handlers.offset = XtOffsetOf(lua_closure_object, std);
	lua_closure_handlers.clone_obj = NULL;
	lua_closure_handlers.free_obj = php_lua_closure_free_obj;
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
