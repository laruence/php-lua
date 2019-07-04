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
#include "php_ini.h"
#include "ext/standard/info.h"
#include "Zend/zend_exceptions.h"
#include "php_lua.h"
#include "lua_closure.h"

zend_class_entry 	*lua_ce;
zend_class_entry    *lua_exception_ce;
static zend_object_handlers lua_object_handlers;

/** {{{ ARG_INFO
 *
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_lua_call, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lua_assign, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lua_register, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, function)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lua_include, 0, 0, 1)
	ZEND_ARG_INFO(0, file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lua_eval, 0, 0, 1)
	ZEND_ARG_INFO(0, statements)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lua_free, 0, 0, 1)
	ZEND_ARG_INFO(0, closure)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ lua_module_entry
*/
zend_module_entry lua_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"lua",
	NULL,
	PHP_MINIT(lua),
	PHP_MSHUTDOWN(lua),
	NULL,
	NULL,
	PHP_MINFO(lua),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_LUA_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_LUA
ZEND_GET_MODULE(lua)
#endif

/** {{{ static void php_lua_stack_dump(lua_State* L)
 *  just for debug
 */
static void php_lua_stack_dump(lua_State* L) {
	int i = 1;
	int n = lua_gettop(L);
	php_printf("The Length of stack is %d\n", n);
	for (; i <= n; ++i) {
		int t = lua_type(L, i);
		php_printf("%s:", lua_typename(L, t));
		switch(t) {
			case LUA_TNUMBER:
				php_printf("%f", lua_tonumber(L, i));
				break;
			case LUA_TSTRING:
				php_printf("%s", lua_tostring(L, i));
				break;
			case LUA_TTABLE:
				break;
			case LUA_TFUNCTION:
				break;
			case LUA_TNIL:
				php_printf("NULL");
				break;
			case LUA_TBOOLEAN:
				php_printf("%s", lua_toboolean(L, i) ? "TRUE" : "FALSE");
				break;
			default:
				break;
		}
		php_printf("\n");
	}
}
/* }}} */

/** {{{ static int php_lua_atpanic(lua_State *L)
*/
static int php_lua_atpanic(lua_State *L) {
	php_error_docref(NULL, E_ERROR, "lua panic (%s)", lua_tostring(L, 1));
	lua_pop(L, 1);
	zend_bailout();
	return 0;
}
/* }}} */

/** {{{ static int php_lua_print(lua_State *L)
*/
static int php_lua_print(lua_State *L)  {
	zval rv;
	int i = 0;
    int nargs = lua_gettop(L);

    for (i = 1; i <= nargs; ++i) {
		php_lua_get_zval_from_lua(L, i, NULL, &rv);
		zend_print_zval_r(&rv, 1);
		zval_ptr_dtor(&rv);
	}
	return 0;
}
/* }}} */

/** {{{ static void * php_lua_alloc_function(void *ud, void *ptr, size_t osize, size_t nsize)
 * the memory-allocation function used by Lua states.
 */
static void * php_lua_alloc_function(void *ud, void *ptr, size_t osize, size_t nsize) {
	(void) osize;
	(void) nsize;

	if (nsize) {
		if (ptr) {
			return erealloc(ptr, nsize);
		}
		return emalloc(nsize);
	} else {
		if (ptr) {
			efree(ptr);
		}
	}

	return NULL;
}
/* }}} */

static void php_lua_dtor_object(zend_object *object) /* {{{ */ {
	php_lua_object *lua_obj = php_lua_obj_from_obj(object);

	zend_object_std_dtor(&(lua_obj->obj));
}
/* }}} */

static void php_lua_free_object(zend_object *object) /* {{{ */ {
	php_lua_object *lua_obj = php_lua_obj_from_obj(object);

	if (lua_obj->L) {
		lua_close(lua_obj->L);
	}
}
/* }}} */

/** {{{ static zend_object_value php_lua_create_object(zend_class_entry *ce)
 */
zend_object *php_lua_create_object(zend_class_entry *ce)
{
	php_lua_object* intern;
	lua_State *L;

	L = luaL_newstate();

	lua_atpanic(L, php_lua_atpanic);

	intern = ecalloc(1, sizeof(php_lua_object) + zend_object_properties_size(ce));
	intern->L = L;

	zend_object_std_init(&intern->obj, ce);
	object_properties_init(&intern->obj, ce);

	intern->obj.handlers = &lua_object_handlers;
	
	return &intern->obj;
}
/* }}} */

/** {{{ static zval * php_lua_read_property(zval *object, zval *member, int type)
*/
zval *php_lua_read_property(zval *object, zval *member, int type, void **cache_slot, zval *rv){
	lua_State *L = (Z_LUAVAL_P(object))->L;
	zend_string *str_member;

	if (type != BP_VAR_R) {
		ZVAL_NULL(rv);
		return rv;
	}

	str_member = zval_get_string(member);
#if (LUA_VERSION_NUM < 502)
	lua_getfield(L, LUA_GLOBALSINDEX, ZSTR_VAL(str_member));
#else
	lua_getglobal(L, ZSTR_VAL(str_member));
#endif
	zend_string_release(str_member);

	php_lua_get_zval_from_lua(L, -1, object, rv);
	lua_pop(L, 1);
	return rv;
}
/* }}} */

/** {{{ static void php_lua_write_property(zval *object, zval *member, zval *value)
*/
static void php_lua_write_property(zval *object, zval *member, zval *value, void ** key) {
	lua_State *L = (Z_LUAVAL_P(object))->L;
	zend_string *str_member = zval_get_string(member);

#if (LUA_VERSION_NUM < 502)
	php_lua_send_zval_to_lua(L, member);
	php_lua_send_zval_to_lua(L, value);

	lua_settable(L, LUA_GLOBALSINDEX);
#else
	php_lua_send_zval_to_lua(L, value);
	lua_setglobal(L, Z_STRVAL_P(member));
#endif

	zend_string_release(str_member);
}
/* }}} */

/** {{{  static int php_lua_call_callback(lua_State *L)
*/
static int php_lua_call_callback(lua_State *L) {
	int  order		 = 0;
	zval retval;
	zval *func		 = NULL;
	zval *callbacks	 = NULL;

	order = lua_tonumber(L, lua_upvalueindex(1));

	callbacks = zend_read_static_property(lua_ce, ZEND_STRL("_callbacks"), 1);
	
	if (ZVAL_IS_NULL(callbacks)) {
		return 0;
	}
	
	func = zend_hash_index_find(Z_ARRVAL_P(callbacks), order);

	if (!zend_is_callable(func, 0, NULL)) {
		return 0;
	} else {
		zval *params;
		int i = 0;
		int arg_num = lua_gettop(L);

		params = safe_emalloc(sizeof(zval), arg_num, 0);
		for (i = 0; i < arg_num; i++) {
			php_lua_get_zval_from_lua(L, -(arg_num - i), NULL, &params[i]);
		}

		call_user_function(EG(function_table), NULL, func, &retval, arg_num, params);
		php_lua_send_zval_to_lua(L, &retval);

		for (i = 0; i<arg_num; i++) {
			zval_ptr_dtor(&params[i]);
			
		}
		efree(params);
		zval_ptr_dtor(&retval);
		return 1;
	}

}
/* }}} */

zval *php_lua_get_zval_from_lua(lua_State *L, int index, zval *lua_obj, zval *rv) /* {{{ */ {
	switch (lua_type(L, index)) {
		case LUA_TNIL:
			ZVAL_NULL(rv);
			break;
		case LUA_TBOOLEAN:
			ZVAL_BOOL(rv, lua_toboolean(L, index));
			break;
		case LUA_TNUMBER:
			ZVAL_DOUBLE(rv, lua_tonumber(L, index));
			break;
		case LUA_TSTRING:
			{
				char *val  = NULL;
				size_t len = 0;

				val = (char *)lua_tolstring(L, index, &len);
				ZVAL_STRINGL(rv, val, len);
			}
			break;
		case LUA_TTABLE:
			array_init(rv);
			lua_pushvalue(L, index);  // table
			lua_pushnil(L);  // first key
			while (lua_next(L, -2) != 0) {
				zval key, val;

				lua_pushvalue(L, -2);

				/* uses 'key' (at index -1) and 'value' (at index -2) */
				if (!php_lua_get_zval_from_lua(L, -1, lua_obj, &key)) {
					break;
				}
				if (!php_lua_get_zval_from_lua(L, -2, lua_obj, &val)) {
					zval_ptr_dtor(&key);
					/* there is a warning already in php_lua_get_zval_from_lua */
					break;
				}

				switch (Z_TYPE(key)) {
					case IS_DOUBLE:
					case IS_LONG:
						add_index_zval(rv, Z_DVAL(key), &val);
						break;
					case IS_STRING:
						add_assoc_zval(rv, Z_STRVAL(key), &val);
						zval_ptr_dtor(&key);
						break;
					case IS_ARRAY:
					case IS_OBJECT:
					default:
						break;
				}
				lua_pop(L, 2);
			}
			lua_pop(L, 1);
			break;
		case LUA_TFUNCTION:
			{
				long ref_id = 0;
				lua_pushvalue(L, index);
				ref_id = luaL_ref(L, LUA_REGISTRYINDEX);

				if (!php_lua_closure_instance(rv, ref_id, lua_obj)) {
					php_error_docref(NULL, E_WARNING, "failed to initialize closure object");
					ZVAL_NULL(rv);
					return NULL;
				}
			}
			break;
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		default:
			php_error_docref(NULL, E_WARNING, "unsupported type '%s' for php",
					lua_typename(L, lua_type(L, index)));
			ZVAL_NULL(rv);
			return NULL;
	}
	return rv;
}
/* }}} */

int php_lua_send_zval_to_lua(lua_State *L, zval *val) /* {{{ */ {

try_again:
	switch (Z_TYPE_P(val)) {
		case IS_TRUE:
			lua_pushboolean(L, 1);
			break;
		case IS_FALSE:
			lua_pushboolean(L, 0);
			break;
		case IS_UNDEF:
		case IS_NULL:
			lua_pushnil(L);
			break;
		case IS_DOUBLE:
			lua_pushnumber(L, Z_DVAL_P(val));
			break;
		case IS_LONG:
			lua_pushnumber(L, Z_LVAL_P(val));
			break;
		case IS_STRING:
			lua_pushlstring(L, Z_STRVAL_P(val), Z_STRLEN_P(val));
			break;
		case IS_OBJECT:
		case IS_ARRAY:
			{
				if (zend_is_callable(val, 0, NULL)) {
					zval *callbacks;

					callbacks = zend_read_static_property(lua_ce, ZEND_STRL("_callbacks"), 1);

					if (ZVAL_IS_NULL(callbacks)) {
						array_init(callbacks);
					}

					lua_pushnumber(L, zend_hash_num_elements(Z_ARRVAL_P(callbacks)));
					lua_pushcclosure(L, php_lua_call_callback, 1);

					zval_add_ref(val);
					add_next_index_zval(callbacks, val);
				} else {
					zval *v;
					ulong longkey;
					zend_string *key;
					zval zkey;

					HashTable *ht = HASH_OF(val);
					if (
#if PHP_VERSION_ID < 70300
						ZEND_HASH_APPLY_PROTECTION(ht)
#else
						!(GC_FLAGS(ht) & GC_IMMUTABLE)
#endif
						) {
						if (
#if PHP_VERSION_ID < 70300
							ZEND_HASH_GET_APPLY_COUNT(ht)
#else
							GC_IS_RECURSIVE(ht)
#endif
							) {

							php_error_docref(NULL, E_ERROR, "recursion found");
							break;
						}
#if PHP_VERSION_ID < 70300
						ZEND_HASH_INC_APPLY_COUNT(ht);
#else
						GC_PROTECT_RECURSION(ht);
#endif
					}

					lua_newtable(L);

					ZEND_HASH_FOREACH_KEY_VAL_IND(ht, longkey, key, v) {
						if (key) {
							ZVAL_STR(&zkey, key);
						} else {
							ZVAL_LONG(&zkey, longkey);
						}
						php_lua_send_zval_to_lua(L, &zkey);
						php_lua_send_zval_to_lua(L, v);
						lua_settable(L, -3);
					} ZEND_HASH_FOREACH_END();

					if (
#if PHP_VERSION_ID < 70300
						ZEND_HASH_APPLY_PROTECTION(ht)
#else
						!(GC_FLAGS(ht) & GC_IMMUTABLE)
#endif
						) {
#if PHP_VERSION_ID < 70300
						ZEND_HASH_DEC_APPLY_COUNT(ht);
#else
						GC_UNPROTECT_RECURSION(ht);
#endif
					}
				}
			}
			break;
		case IS_REFERENCE:
			ZVAL_DEREF(val);
			goto try_again;
			break;
		case IS_INDIRECT:
			val = Z_INDIRECT_P(val);
			goto try_again;
			break;
		default:
			php_error_docref(NULL, E_ERROR, "unsupported type `%s' for lua", zend_zval_type_name(val));
			lua_pushnil(L);
			return 1;
	}

	return 0;
}
/* }}} */

/*** {{{ static int php_lua_arg_apply_func(void *data, void *L)
*/
static int php_lua_arg_apply_func(void *data, void *L) {
	php_lua_send_zval_to_lua((lua_State*)L, (zval*)data);
	return ZEND_HASH_APPLY_KEEP;
} /* }}} */

static zval *php_lua_call_lua_function(zval *lua_obj, zval *func, zval *args, int use_self, zval *retval) /* {{{ */ {
	int bp = 0;
	int sp = 0;
	int arg_num = 0;
	zval rv;
	lua_State *L;

	L = (Z_LUAVAL_P(lua_obj))->L;

	if (IS_ARRAY == Z_TYPE_P(func)) {
		zval *t, *f;
		if ((t = zend_hash_index_find(Z_ARRVAL_P(func), 0)) == NULL || Z_TYPE_P(t) != IS_STRING
				|| (f = zend_hash_index_find(Z_ARRVAL_P(func), 1)) == NULL || Z_TYPE_P(f) != IS_STRING) {
			/* as johannes suggesting use exceptioni to distinguish the error from a lua function return false
			   php_error_docref(NULL, E_WARNING,
			   "invalid lua function, argument must be an array which contain two elements: array('table', 'method')");
			   */
			zend_throw_exception_ex(lua_exception_ce, 0,
					"invalid lua function, argument must be an array which contain two elements: array('table', 'method')");
			return NULL;
		}
#if (LUA_VERSION_NUM < 502)
		lua_getfield(L, LUA_GLOBALSINDEX, Z_STRVAL_P(t));
#else
		lua_getglobal(L, Z_STRVAL_P(t));
#endif
		if (LUA_TTABLE != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -1);
			zend_throw_exception_ex(lua_exception_ce, 0, "invalid lua table '%s'", Z_STRVAL_P(t));
			return NULL;
		}
		bp = lua_gettop(L);
		lua_getfield(L, -1, Z_STRVAL_P(f));
		if (LUA_TFUNCTION != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -2);
			zend_throw_exception_ex(lua_exception_ce, 0, "invalid lua table function '%s'.%s", Z_STRVAL_P(t), Z_STRVAL_P(f));
			return NULL;
		}
	} else if (IS_STRING == Z_TYPE_P(func)) {
		bp = lua_gettop(L);
#if (LUA_VERSION_NUM < 502)
		lua_getfield(L, LUA_GLOBALSINDEX, Z_STRVAL_P(func));
#else
		lua_getglobal(L, Z_STRVAL_P(func));
#endif
		if (LUA_TFUNCTION != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -1);
			zend_throw_exception_ex(lua_exception_ce, 0, "invalid lua function '%s'", Z_STRVAL_P(func));
			return NULL;
		}
	} else if (IS_OBJECT == Z_TYPE_P(func)
			&& instanceof_function(Z_OBJCE_P(func), php_lua_get_closure_ce())) {
		lua_closure_object *closure_obj = php_lua_closure_object_from_zend_object(Z_OBJ_P(func));
		bp = lua_gettop(L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, closure_obj->closure);
		if (LUA_TFUNCTION != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -1);
			zend_throw_exception_ex(lua_exception_ce, 0, "call to lua closure failed");
			return NULL;
		}
	}

	if (use_self) {
		lua_pushvalue(L, -2);
		lua_remove(L, -2);
		arg_num++;
	}

	if (args) {
		arg_num += zend_hash_num_elements(Z_ARRVAL_P(args));
		zend_hash_apply_with_argument(Z_ARRVAL_P(args), (apply_func_arg_t)php_lua_arg_apply_func, (void *)L);
	}

	if (lua_pcall(L, arg_num, LUA_MULTRET, 0) != LUA_OK) {
		php_error_docref(NULL, E_WARNING,
				"call to lua function %s failed", lua_tostring(L, -1));
		lua_pop(L, lua_gettop(L) - bp);
		return NULL;
	}

	sp = lua_gettop(L) - bp;

	if (!sp) {
		ZVAL_NULL(retval);
	} else if (sp == 1) {
		php_lua_get_zval_from_lua(L, -1, lua_obj, retval);
	} else {
		zval rv;
		int  i = 0;
		array_init(retval);
		for (i = -sp; i < 0; i++) {
			php_lua_get_zval_from_lua(L, i, lua_obj, &rv);
			add_next_index_zval(retval, &rv);
		}
	}

	lua_pop(L, sp);

	if (IS_ARRAY == Z_TYPE_P(func)) {
		lua_pop(L, -1);
	}

	return retval;
} /* }}} */

/** {{{ proto Lua::eval(string $lua_chunk)
 * eval a lua chunk
 */
PHP_METHOD(lua, eval) {
	lua_State *L;
	char *statements;
	long bp, len;
	int ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &statements, &len) == FAILURE) {
		return;
	}

	L = (Z_LUAVAL_P(getThis()))->L;

	bp = lua_gettop(L);
	if ((ret = luaL_loadbuffer(L, statements, len, "line")) != LUA_OK || (ret = lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK)) {
		zend_throw_exception_ex(lua_exception_ce, ret, "%s", lua_tostring(L, -1));
		lua_pop(L, 1);
		RETURN_FALSE;
	} else {
		int ret_count;

		ret_count = lua_gettop(L) - bp;
		if (ret_count > 1) {
			zval rv;
			int i = 0;
			array_init(return_value);
			for (i = -ret_count; i<0; i++) {
				php_lua_get_zval_from_lua(L, i, getThis(), &rv);
				add_next_index_zval(return_value, &rv);
			}
		} else if (ret_count) {
			php_lua_get_zval_from_lua(L, -1, getThis(), return_value);
		}
		lua_pop(L, ret_count);
	}
}
/* }}} */

/** {{{ proto Lua::include(string $file)
 * run a lua script file
 */
PHP_METHOD(lua, include) {
	lua_State *L;
	char *file;
	size_t bp, len;
	int ret;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &file, &len) == FAILURE) {
		return;
	}

	if (php_check_open_basedir(file)) {
		RETURN_FALSE;
	}

	L = (Z_LUAVAL_P(getThis()))->L;

	bp = lua_gettop(L);
	if ((ret = luaL_loadfile(L, file)) != LUA_OK || (ret = lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK)) {
		zend_throw_exception_ex(lua_exception_ce, ret, "%s", lua_tostring(L, -1));
		lua_pop(L, 1);
		RETURN_FALSE;
	} else {
		int ret_count;

		ret_count = lua_gettop(L) - bp;
		if (ret_count > 1) {
			zval rv;
			int i = 0;
			array_init(return_value);
			for (i = -ret_count; i<0; i++) {
				php_lua_get_zval_from_lua(L, i, getThis(), &rv);
				add_next_index_zval(return_value, &rv);
			}
		} else if (ret_count) {
			php_lua_get_zval_from_lua(L, -1, getThis(), return_value);
		}
		lua_pop(L, ret_count);
	}
}
/* }}} */

/** {{{ proto Lua::call(mixed $function, array $args, bool $use_self)
*/
PHP_METHOD(lua, call) {
	long u_self = 0;
	zval *func;
	zval *args	= NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|al", &func, &args, &u_self) == FAILURE) {
		return;
	}

	if (!(php_lua_call_lua_function(getThis(), func, args, u_self, return_value))) {
		RETURN_FALSE;
	}
}
/* }}} */

/** {{{ proto Lua::assign(string $name, mix $value)
*/
PHP_METHOD(lua, assign) {
	zval *name;
	zval *value;
	lua_State *L;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz", &name,  &value) == FAILURE) {
		return;
	}

	L = (Z_LUAVAL_P(getThis()))->L;

	php_lua_send_zval_to_lua(L, value);
#if (LUA_VERSION_NUM < 502)
	lua_setfield(L, LUA_GLOBALSINDEX, Z_STRVAL_P(name));
#else
	lua_setglobal(L, Z_STRVAL_P(name));
#endif

	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{ proto Lua::registerCallback(string $name, mix $value)
*/
PHP_METHOD(lua, registerCallback) {
	char *name;
	size_t len;
	zval *func;
	lua_State *L;
	zval* callbacks;

	if (zend_parse_parameters(ZEND_NUM_ARGS(),"sz", &name, &len, &func) == FAILURE) {
		return;
	}

	L = (Z_LUAVAL_P(getThis()))->L;

	callbacks = zend_read_static_property(lua_ce, ZEND_STRL("_callbacks"), 1);

	if (ZVAL_IS_NULL(callbacks)) {
		array_init(callbacks);
	}

	if (zend_is_callable(func, 0, NULL)) {
		lua_pushnumber(L, zend_hash_num_elements(Z_ARRVAL_P(callbacks)));
		lua_pushcclosure(L, php_lua_call_callback, 1);
		lua_setglobal(L, name);
	} else {
		zend_throw_exception_ex(lua_exception_ce, 0, "invalid php callback");
		RETURN_FALSE;
	}

	zval_add_ref(func);
	add_next_index_zval(callbacks, func);

	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{ proto Lua::getVersion()
*/
PHP_METHOD(lua, getVersion) {
	RETURN_STRING(LUA_RELEASE);
}
/* }}} */

/** {{{ proto Lua::__construct()
*/
PHP_METHOD(lua, __construct) {
	lua_State * L = (Z_LUAVAL_P(getThis()))->L;
	
	luaL_openlibs(L);
	lua_register(L, "print", php_lua_print);
	if (ZEND_NUM_ARGS()) {
		PHP_MN(lua_include)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
	}
}
/* }}} */

/* {{{ lua_class_methods[]
 *
 */
zend_function_entry lua_class_methods[] = {
	PHP_ME(lua, __construct,		NULL,  					ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(lua, eval,          		arginfo_lua_eval,  		ZEND_ACC_PUBLIC)
	PHP_ME(lua, include,			arginfo_lua_include, 	ZEND_ACC_PUBLIC)
	PHP_ME(lua, call,				arginfo_lua_call,  		ZEND_ACC_PUBLIC)
	PHP_ME(lua, assign,				arginfo_lua_assign,		ZEND_ACC_PUBLIC)
	PHP_ME(lua, getVersion,			NULL, 					ZEND_ACC_PUBLIC|ZEND_ACC_ALLOW_STATIC)
	PHP_ME(lua, registerCallback,	arginfo_lua_register, 	ZEND_ACC_PUBLIC)
	PHP_MALIAS(lua, __call, call, 	arginfo_lua_call,		ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
*/
PHP_MINIT_FUNCTION(lua) {
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Lua", lua_class_methods);

	REGISTER_LONG_CONSTANT("LUA_OK", LUA_OK, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("LUA_YIELD", LUA_YIELD, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("LUA_ERRRUN", LUA_ERRRUN, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("LUA_ERRSYNTAX", LUA_ERRSYNTAX, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("LUA_ERRMEM", LUA_ERRMEM, CONST_PERSISTENT | CONST_CS);
#ifdef LUA_ERRGCMM
	REGISTER_LONG_CONSTANT("LUA_ERRGCMM", LUA_ERRGCMM, CONST_PERSISTENT | CONST_CS);
#endif
	REGISTER_LONG_CONSTANT("LUA_ERRERR", LUA_ERRERR, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("LUA_ERRFILE", LUA_ERRFILE, CONST_PERSISTENT | CONST_CS);


	lua_ce = zend_register_internal_class(&ce);

	lua_ce->create_object = php_lua_create_object;
	memcpy(&lua_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	lua_object_handlers.offset = XtOffsetOf(php_lua_object, obj);
	lua_object_handlers.dtor_obj = php_lua_dtor_object;
	lua_object_handlers.free_obj = php_lua_free_object;
	lua_object_handlers.clone_obj = NULL;
	lua_object_handlers.write_property = php_lua_write_property;
	lua_object_handlers.read_property  = php_lua_read_property;

	lua_ce->ce_flags |= ZEND_ACC_FINAL;

	zend_declare_property_null(lua_ce, ZEND_STRL("_callbacks"), ZEND_ACC_STATIC|ZEND_ACC_PRIVATE);
	zend_declare_class_constant_string(lua_ce, ZEND_STRL("LUA_VERSION"), LUA_RELEASE);

	php_lua_closure_register();

	INIT_CLASS_ENTRY(ce, "LuaException", NULL);

	lua_exception_ce = zend_register_internal_class_ex(&ce, zend_exception_get_default());

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
*/
PHP_MSHUTDOWN_FUNCTION(lua)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
*/
PHP_MINFO_FUNCTION(lua)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "lua support", "enabled");
	php_info_print_table_row(2, "lua extension version", PHP_LUA_VERSION);
	php_info_print_table_row(2, "lua release", LUA_RELEASE);
	php_info_print_table_row(2, "lua copyright", LUA_COPYRIGHT);
	php_info_print_table_row(2, "lua authors", LUA_AUTHORS);
	php_info_print_table_end();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
