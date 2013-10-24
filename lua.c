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
   +----------------------------------------------------------------------+
   $Id: lua.c 324348 2012-03-19 03:12:15Z laruence $
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
#ifdef PHP_LUA_DEBUG
static void php_lua_stack_dump(lua_State* L) {
	int i = 1;
	int n = lua_gettop(L);
	printf("The Length of stack is %d\n", n);
	for (; i <= n; ++i) {
		int t = lua_type(L, i);
		printf("%s:", lua_typename(L, t));
		switch(t) {
			case LUA_TNUMBER:
				printf("%f", lua_tonumber(L, i));
				break;
			case LUA_TSTRING:
				printf("%s", lua_tostring(L, i));
				break;
			case LUA_TTABLE:
				break;
			case LUA_TFUNCTION:
				break;
			case LUA_TNIL:
				printf("NULL");
				break;
			case LUA_TBOOLEAN:
				printf("%s", lua_toboolean(L, i) ? "TRUE" : "FALSE");
				break;
			default:
				break;
		}
		printf("\n");
	}
}
#endif
/* }}} */

/** {{{ static int php_lua_atpanic(lua_State *L)
*/
static int php_lua_atpanic(lua_State *L) {
	TSRMLS_FETCH();
	php_error_docref(NULL TSRMLS_CC, E_ERROR, "lua panic (%s)", lua_tostring(L, 1));
	lua_pop(L, 1);
	zend_bailout();
	return 0;
}
/* }}} */

/** {{{ static int php_lua_print(lua_State *L)
*/
static int php_lua_print(lua_State *L)  {
	int i = 0;

	TSRMLS_FETCH();
	for (i = -lua_gettop(L) ; i<0; i++) {
		zval *tmp = php_lua_get_zval_from_lua(L, i, NULL TSRMLS_CC);
		zend_print_zval_r(tmp, 1 TSRMLS_CC);
		zval_ptr_dtor(&tmp);
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

/** {{{ static void php_lua_dtor_object(void *object, zend_object_handle handle TSRMLS_DC)
 *  the dtor function for lua object
 */
static void php_lua_dtor_object(void *object, zend_object_handle handle TSRMLS_DC) {
	php_lua_object *lua_obj = (php_lua_object *)object;

	zend_object_std_dtor(&(lua_obj->obj) TSRMLS_CC);

	if (lua_obj->L) {
		lua_close(lua_obj->L);
	}

	efree(lua_obj);
}
/* }}} */

/** {{{ static zend_object_value php_lua_create_object(zend_class_entry *ce TSRMLS_DC)
 *
 * the create object handler for lua
 */
static zend_object_value php_lua_create_object(zend_class_entry *ce TSRMLS_DC) {
	zend_object_value obj 	 = {0};
	php_lua_object 	*lua_obj = NULL;
	lua_State 		*L	 	 = NULL;

	L = lua_newstate(php_lua_alloc_function, NULL);

	lua_atpanic(L, php_lua_atpanic);

	lua_obj = emalloc(sizeof(php_lua_object));

	if (!lua_obj) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "alloc memory for lua object failed");
	}

	lua_obj->L = L;
	zend_object_std_init(&(lua_obj->obj), ce TSRMLS_CC);

#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)
	zend_hash_copy(lua_obj->obj.properties, &ce->default_properties,
#if (PHP_MINOR_VERSION < 3)
			(copy_ctor_func_t) zval_add_ref,
#else
			zval_copy_property_ctor(ce),
#endif
		(void *)0, sizeof(zval *));
#elif (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION > 3)
	object_properties_init(&(lua_obj->obj), ce);
#endif

	obj.handle   = zend_objects_store_put(lua_obj, php_lua_dtor_object, NULL, NULL TSRMLS_CC);
	obj.handlers = &lua_object_handlers;

	return obj;
}
/* }}} */

/** {{{ static zval * php_lua_read_property(zval *object, zval *member, int type TSRMLS_DC)
*/
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)) || (PHP_MAJOR_VERSION < 5)
static zval * php_lua_read_property(zval *object, zval *member, int type TSRMLS_DC) {
#else
static zval * php_lua_read_property(zval *object, zval *member, int type, const struct _zend_literal *key TSRMLS_DC) {
#endif
	zval *retval 	 = NULL;
	lua_State *L 	 = NULL;
	zval *tmp_member = NULL;
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION > 3))
	(void)key;
#endif

	if (type != BP_VAR_R) {
		MAKE_STD_ZVAL(retval);
		ZVAL_NULL(retval);
		return retval;
	}

	if (Z_TYPE_P(member) != IS_STRING) {
		ALLOC_ZVAL(tmp_member);
		*tmp_member = *member;
		INIT_PZVAL(tmp_member);
		zval_copy_ctor(tmp_member);
		convert_to_string(tmp_member);
		member = tmp_member;
	}

	L = Z_LUAVAL_P(object);
#if (LUA_VERSION_NUM < 502)
	lua_getfield(L, LUA_GLOBALSINDEX, Z_STRVAL_P(member));
#else
	lua_getglobal(L, Z_STRVAL_P(member));
#endif
	retval = php_lua_get_zval_from_lua(L, -1, object TSRMLS_CC);
	Z_DELREF_P(retval);
	lua_pop(L, 1);

	if (tmp_member) {
		zval_ptr_dtor(&tmp_member);
	}

	return retval;
}
/* }}} */

/** {{{ static void php_lua_write_property(zval *object, zval *member, zval *value TSRMLS_DC)
*/
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)) || (PHP_MAJOR_VERSION < 5)
static void php_lua_write_property(zval *object, zval *member, zval *value TSRMLS_DC) {
#else
static void php_lua_write_property(zval *object, zval *member, zval *value, const struct _zend_literal *key TSRMLS_DC) {
#endif
	lua_State *L 	 = NULL;
	zval *tmp_member = NULL;
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION > 3))
	(void)key;
#endif

	if (Z_TYPE_P(member) != IS_STRING) {
		ALLOC_ZVAL(tmp_member);
		*tmp_member = *member;
		INIT_PZVAL(tmp_member);
		zval_copy_ctor(tmp_member);
		convert_to_string(tmp_member);
		member = tmp_member;
	}

	L = Z_LUAVAL_P(object);

#if (LUA_VERSION_NUM < 502)
	php_lua_send_zval_to_lua(L, member TSRMLS_CC);
	php_lua_send_zval_to_lua(L, value TSRMLS_CC);

	lua_settable(L, LUA_GLOBALSINDEX);
#else
	php_lua_send_zval_to_lua(L, value TSRMLS_CC);
	lua_setglobal(L, Z_STRVAL_P(member));
#endif

	if (tmp_member) {
		zval_ptr_dtor(&tmp_member);
	}
}
/* }}} */

/** {{{  static int php_lua_call_callback(lua_State *L)
*/
static int php_lua_call_callback(lua_State *L) {
	int  order		 = 0;
	zval *return_value = NULL;
	zval **func		 = NULL;
	zval *callbacks	 = NULL;
	TSRMLS_FETCH();

	order = lua_tonumber(L, lua_upvalueindex(1));

	callbacks = zend_read_static_property(lua_ce, ZEND_STRL("_callbacks"), 1 TSRMLS_CC);

	if (ZVAL_IS_NULL(callbacks)) {
		return 0;
	}

	MAKE_STD_ZVAL(return_value);

	if (zend_hash_index_find(Z_ARRVAL_P(callbacks), order, (void **)&func) == FAILURE) {
		return 0;
	}

	if (!zend_is_callable(*func, 0, NULL TSRMLS_CC)) {
		return 0;
	} else {
		zval **params = NULL;
		int  i 		= 0;
		int  arg_num  = lua_gettop(L);

		params = ecalloc(arg_num, sizeof(zval));

		for (; i<arg_num; i++) {
			params[i] = php_lua_get_zval_from_lua(L, -(arg_num-i), NULL TSRMLS_CC);
		}

		call_user_function(EG(function_table), NULL, *func, return_value, arg_num, params TSRMLS_CC);

		php_lua_send_zval_to_lua(L, return_value TSRMLS_CC);

		for (i=0; i<arg_num; i++) {
			zval_ptr_dtor(&params[i]);
		}

		efree(params);
		zval_ptr_dtor(&return_value);

		return 1;
	}

}
/* }}} */

/** {{{ zval * php_lua_get_zval_from_lua(lua_State *L, int index, zval *lua_obj TSRMLS_DC)
*/
zval * php_lua_get_zval_from_lua(lua_State *L, int index, zval *lua_obj TSRMLS_DC) {
	zval *retval;

	MAKE_STD_ZVAL(retval);
	ZVAL_NULL(retval);

	switch (lua_type(L, index)) {
		case LUA_TNIL:
			ZVAL_NULL(retval);
			break;
		case LUA_TBOOLEAN:
			ZVAL_BOOL(retval, lua_toboolean(L, index));
			break;
		case LUA_TNUMBER:
			ZVAL_DOUBLE(retval, lua_tonumber(L, index));
			break;
		case LUA_TSTRING:
			{
				char *val  = NULL;
				size_t len = 0;

				val = (char *)lua_tolstring(L, index, &len);
				ZVAL_STRINGL(retval, val, len, 1);
			}
			break;
		case LUA_TTABLE:
			array_init(retval);
			lua_pushnil(L);  /* first key */
			while (lua_next(L, index-1) != 0) {
				zval *key = NULL;
				zval *val = NULL;

				/* uses 'key' (at index -2) and 'value' (at index -1) */
				key = php_lua_get_zval_from_lua(L, -2, lua_obj TSRMLS_CC);
				val = php_lua_get_zval_from_lua(L, -1, lua_obj TSRMLS_CC);

				if (!key || !val) {
					/* there is a warning already in php_lua_get_zval_from_lua */
					break;
				}

				switch(Z_TYPE_P(key)) {
					case IS_DOUBLE:
					case IS_LONG:
						add_index_zval(retval, Z_DVAL_P(key), val);
						break;
					case IS_STRING:
						add_assoc_zval(retval, Z_STRVAL_P(key), val);
						break;
					case IS_ARRAY:
					case IS_OBJECT:
					default:
						break;
				}
				lua_pop(L, 1);
				zval_ptr_dtor(&key);
			}
			break;
		case LUA_TFUNCTION:
			{
				long ref_id   = 0;
				if (!lua_obj) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "corrupted Lua object");
					break;
				}

				lua_pushvalue(L, index);
				ref_id = luaL_ref(L, LUA_REGISTRYINDEX);

				if (!php_lua_closure_instance(retval, ref_id, lua_obj TSRMLS_CC)) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to initialize closure object");
				}
			}
			break;
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unsupported type '%s' for php",
					lua_typename(L, lua_type(L, index)));
	}

	return retval;
}
/* }}} */

/** {{{ int php_lua_send_zval_to_lua(lua_State *L, zval *val TSRMLS_DC)
*/
int php_lua_send_zval_to_lua(lua_State *L, zval *val TSRMLS_DC) {

	switch (Z_TYPE_P(val)) {
		case IS_BOOL:
			lua_pushboolean(L, Z_BVAL_P(val));
			break;
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
				if (zend_is_callable(val, 0, NULL TSRMLS_CC)) {
					zval* callbacks = NULL;

					callbacks = zend_read_static_property(lua_ce, ZEND_STRL("_callbacks"), 1 TSRMLS_CC);

					if (ZVAL_IS_NULL(callbacks)) {
						array_init(callbacks);
					}

					lua_pushnumber(L, zend_hash_num_elements(Z_ARRVAL_P(callbacks)));
					lua_pushcclosure(L, php_lua_call_callback, 1);

					zval_add_ref(&val);
					add_next_index_zval(callbacks, val);
				} else {
					HashTable *ht  		= NULL;
					zval 		**ppzval 	= NULL;

					ht = HASH_OF(val);

					if (++ht->nApplyCount > 1) {
						php_error_docref(NULL TSRMLS_CC, E_ERROR, "recursion found");
						--ht->nApplyCount;
						break;
					}

					lua_newtable(L);
					for(zend_hash_internal_pointer_reset(ht);
							zend_hash_get_current_data(ht, (void **)&ppzval) == SUCCESS;
							zend_hash_move_forward(ht)) {
						char *key = NULL;
						uint len  = 0;
						ulong idx  = 0;
						zval *zkey= NULL;

						switch(zend_hash_get_current_key_ex(ht, &key, &len, &idx, 0, NULL)) {
							case HASH_KEY_IS_STRING :
								MAKE_STD_ZVAL(zkey);
								ZVAL_STRINGL(zkey, key, len - 1, 1);
								break;
							case HASH_KEY_IS_LONG:
								if (idx == 0) {
									php_error_docref(NULL TSRMLS_CC, E_STRICT,
											"attempt to pass an array index begin with 0 to lua");
								}
								MAKE_STD_ZVAL(zkey);
								ZVAL_LONG(zkey, idx);
								break;
						}

						php_lua_send_zval_to_lua(L, zkey TSRMLS_CC);
						php_lua_send_zval_to_lua(L, *ppzval TSRMLS_CC);
						lua_settable(L, -3);

						zval_ptr_dtor(&zkey);
					}
					--ht->nApplyCount;
				}
			}
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "unsupported type `%s' for lua"
					, zend_zval_type_name(val));
			lua_pushnil(L);
			return 1;
	}

	return 0;
}
/* }}} */

/*** {{{ static int php_lua_arg_apply_func(void *data, void *L TSRMLS_DC)
*/
static int php_lua_arg_apply_func(void *data, void *L TSRMLS_DC) {
	php_lua_send_zval_to_lua((lua_State*)L, *(zval**)data TSRMLS_CC);
	return ZEND_HASH_APPLY_KEEP;
} /* }}} */

/** {{{ static zval * php_lua_call_lua_function(zval *lua_obj, zval *func, zval *args, int use_self TSRMLS_DC)
*/
static zval * php_lua_call_lua_function(zval *lua_obj, zval *func, zval *args, int use_self TSRMLS_DC) {
	int bp 		= 0;
	int sp 		= 0;
	int arg_num = 0;
	zval *ret   = NULL;
	lua_State *L = NULL;

	L = Z_LUAVAL_P(lua_obj);

	if (IS_ARRAY == Z_TYPE_P(func)) {
		zval **t = NULL;
		zval **f = NULL;
		if (zend_hash_index_find(Z_ARRVAL_P(func), 0, (void **)&t) == FAILURE || Z_TYPE_PP(t) != IS_STRING
				|| zend_hash_index_find(Z_ARRVAL_P(func), 1, (void **)&f) == FAILURE || Z_TYPE_PP(f) != IS_STRING) {
			/* as johannes suggesting use exceptioni to distinguish the error from a lua function return false
			   php_error_docref(NULL TSRMLS_CC, E_WARNING,
			   "invalid lua function, argument must be an array which contain two elements: array('table', 'method')");
			   */
			zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC,
					"invalid lua function, argument must be an array which contain two elements: array('table', 'method')");
			return NULL;
		}
#if (LUA_VERSION_NUM < 502)
		lua_getfield(L, LUA_GLOBALSINDEX, Z_STRVAL_PP(t));
#else
		lua_getglobal(L, Z_STRVAL_PP(t));
#endif
		if (LUA_TTABLE != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -1);
			zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "invalid lua table '%s'", Z_STRVAL_PP(t));
			return NULL;
		}
		bp = lua_gettop(L);
		lua_getfield(L, -1, Z_STRVAL_PP(f));
		if (LUA_TFUNCTION != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -2);
			zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "invalid lua table function '%s'.%s", Z_STRVAL_PP(t), Z_STRVAL_PP(f));
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
			zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "invalid lua function '%s'", Z_STRVAL_P(func));
			return NULL;
		}
	} else if (IS_OBJECT == Z_TYPE_P(func)
			&& instanceof_function(Z_OBJCE_P(func), php_lua_get_closure_ce() TSRMLS_CC)) {
		zval *closure = zend_read_property(php_lua_get_closure_ce(), func, ZEND_STRL("_closure"), 1 TSRMLS_CC);
		if (!Z_LVAL_P(closure)) {
			zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "invalid lua closure");
			return NULL;
		}
		bp = lua_gettop(L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, Z_LVAL_P(closure));
		if (LUA_TFUNCTION != lua_type(L, lua_gettop(L))) {
			lua_pop(L, -1);
			zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "call to lua closure failed");
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
		zend_hash_apply_with_argument(Z_ARRVAL_P(args), php_lua_arg_apply_func, (void *)L TSRMLS_CC);
	}

	if (lua_pcall(L, arg_num, LUA_MULTRET, 0) != LUA_OK) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"call to lua function %s failed", lua_tostring(L, -1));
		lua_pop(L, lua_gettop(L) - bp);
		return NULL;
	}

	sp = lua_gettop(L) - bp;

	if (!sp) {
		MAKE_STD_ZVAL(ret);
		ZVAL_NULL(ret);
	} else if (sp == 1) {
		ret = php_lua_get_zval_from_lua(L, -1, lua_obj TSRMLS_CC);
	} else {
		int  i = 0;
		MAKE_STD_ZVAL(ret);
		array_init(ret);
		for (i = -sp; i < 0; i++) {
			zval *tmp = php_lua_get_zval_from_lua(L, i, lua_obj TSRMLS_CC);
			add_next_index_zval(ret, tmp);
		}
	}

	lua_pop(L, sp);

	if (IS_ARRAY == Z_TYPE_P(func)) {
		lua_pop(L, -1);
	}

	return ret;
} /* }}} */

/** {{{ proto Lua::eval(string $lua_chunk)
 * eval a lua chunk
 */
PHP_METHOD(lua, eval) {
	lua_State *L	 = NULL;
	char *statements = NULL;
	long bp, len	 = 0;
	int ret;

	L = Z_LUAVAL_P(getThis());
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &statements, &len) == FAILURE) {
		return;
	}

	bp = lua_gettop(L);
	if ((ret = luaL_loadbuffer(L, statements, len, "line")) != LUA_OK || (ret = lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK)) {
		zend_throw_exception_ex(lua_exception_ce, ret TSRMLS_CC, "%s", lua_tostring(L, -1));
		lua_pop(L, 1);
		RETURN_FALSE;
	} else {
		zval *tmp 		= NULL;
		int   ret_count	= 0;
		int   i   		= 0;

		ret_count = lua_gettop(L) - bp;
		if (ret_count > 1) {
			array_init(return_value);
			for (i = -ret_count; i<0; i++) {
				tmp = php_lua_get_zval_from_lua(L, i, getThis() TSRMLS_CC);
				add_next_index_zval(return_value, tmp);
			}
		} else if (ret_count) {
			zval *tmp = php_lua_get_zval_from_lua(L, -1, getThis() TSRMLS_CC);
			RETURN_ZVAL(tmp, 1, 1);
		}
		lua_pop(L, ret_count);
	}
}
/* }}} */

/** {{{ proto Lua::include(string $file)
 * run a lua script file
 */
PHP_METHOD(lua, include) {
	lua_State *L = NULL;
	char *file   = NULL;
	long bp, len = 0;
	int ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &file, &len) == FAILURE) {
		return;
	}

	if (php_check_open_basedir(file TSRMLS_CC)
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)) || (PHP_MAJOR_VERSION < 5)
			|| (PG(safe_mode)
				&& !php_checkuid(file, "rb+", CHECKUID_CHECK_MODE_PARAM))
#endif
	   ) {
		RETURN_FALSE;
	}

	L = Z_LUAVAL_P(getThis());

	bp = lua_gettop(L);
	if ((ret = luaL_loadfile(L, file)) != LUA_OK || (ret = lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK)) {
		zend_throw_exception_ex(lua_exception_ce, ret TSRMLS_CC, "%s", lua_tostring(L, -1));
		lua_pop(L, 1);
		RETURN_FALSE;
	}  else {
		zval *tmp 		= NULL;
		int   ret_count	= 0;
		int   i   		= 0;

		ret_count = lua_gettop(L) - bp;
		if (ret_count > 1) {
			array_init(return_value);

			for (i = -ret_count; i<0; i++) {
				tmp = php_lua_get_zval_from_lua(L, i, getThis() TSRMLS_CC);
				add_next_index_zval(return_value, tmp);
			}

		} else if (ret_count) {
			zval *tmp = php_lua_get_zval_from_lua(L, -1, getThis() TSRMLS_CC);
			RETURN_ZVAL(tmp, 1, 1);
		}

		lua_pop(L, ret_count);
	}
}
/* }}} */

/** {{{ proto Lua::call(mixed $function, array $args, bool $use_self)
*/
PHP_METHOD(lua, call) {
	long u_self = 0;
	zval *args	= NULL;
	zval *func  = NULL;
	zval *ret   = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|al", &func, &args, &u_self) == FAILURE) {
		return;
	}

	if ((ret = php_lua_call_lua_function(getThis(), func, args, u_self TSRMLS_CC))) {
		RETURN_ZVAL(ret, 1, 1);
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto Lua::assign(string $name, mix $value)
*/
PHP_METHOD(lua, assign) {
	char *name   = NULL;
	zval *value	 = NULL;
	lua_State *L = NULL;
	int  len 	 = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &len, &value) == FAILURE) {
		return;
	}

	L = Z_LUAVAL_P(getThis());

	php_lua_send_zval_to_lua(L, value TSRMLS_CC);
#if (LUA_VERSION_NUM < 502)
	lua_setfield(L, LUA_GLOBALSINDEX, name);
#else
	lua_setglobal(L, name);
#endif

	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{ proto Lua::registerCallback(string $name, mix $value)
*/
PHP_METHOD(lua, registerCallback) {
	char *name = NULL;
	long len   = 0;
	zval *func = NULL;
	lua_State *L    = NULL;
	zval* callbacks = NULL;
	L = Z_LUAVAL_P(getThis());

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"sz", &name, &len, &func) == FAILURE) {
		return;
	}

	callbacks = zend_read_static_property(lua_ce, ZEND_STRL("_callbacks"), 1 TSRMLS_CC);

	if (ZVAL_IS_NULL(callbacks)) {
		array_init(callbacks);
	}

	if (zend_is_callable(func, 0, NULL TSRMLS_CC)) {
		lua_pushnumber(L, zend_hash_num_elements(Z_ARRVAL_P(callbacks)));
		lua_pushcclosure(L, php_lua_call_callback, 1);
		lua_setglobal(L, name);
	} else {
		zend_throw_exception_ex(lua_exception_ce, 0 TSRMLS_CC, "invalid php callback");
		RETURN_FALSE;
	}

	zval_add_ref(&func);
	add_next_index_zval(callbacks, func);

	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{ proto Lua::getVersion()
*/
PHP_METHOD(lua, getVersion) {
	RETURN_STRING(LUA_RELEASE, 1);
}
/* }}} */

/** {{{ proto Lua::__construct()
*/
PHP_METHOD(lua, __construct) {
	lua_State *L = Z_LUAVAL_P(getThis());
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
	{NULL, NULL, NULL}
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


	lua_ce = zend_register_internal_class(&ce TSRMLS_CC);
	lua_ce->create_object = php_lua_create_object;
	memcpy(&lua_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	lua_object_handlers.write_property = php_lua_write_property;
	lua_object_handlers.read_property  = php_lua_read_property;

	lua_ce->ce_flags |= ZEND_ACC_FINAL;

	zend_declare_property_null(lua_ce, ZEND_STRL("_callbacks"), ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_class_constant_string(lua_ce, ZEND_STRL("LUA_VERSION"), LUA_RELEASE TSRMLS_CC);

	php_lua_closure_register(TSRMLS_C);

	INIT_CLASS_ENTRY(ce, "LuaException", NULL);
	lua_exception_ce = zend_register_internal_class_ex(&ce,
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
			zend_exception_get_default(),
#else
			zend_exception_get_default(TSRMLS_C),
#endif
			NULL  TSRMLS_CC);

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
