dnl $Id: config.m4 321796 2012-01-05 17:23:48Z laruence $
PHP_ARG_WITH(lua, for lua support,
[  --with-lua=[DIR]    Include php lua support])
PHP_ARG_WITH(lua-version, to specify a custom lua version, [  --with-lua-version=[VERSION]]   Use the specified lua version.)

if test "$PHP_LUA" != "no"; then
  if test -r $PHP_LUA/include/lua.h; then
    LUA_INCLUDE_DIR=$PHP_LUA
  else
    AC_MSG_CHECKING(for lua in default path)
    for i in /usr/local /usr; do
      if test -r $i/include/lua/lua.h; then
        LUA_INCLUDE_DIR=$i/include/lua
        AC_MSG_RESULT(found in $i)
        break
      fi

      if test "$PHP_LUA_VERSION" != "yes"; then
        if test -r $i/include/lua$PHP_LUA_VERSION/lua.h; then
          LUA_INCLUDE_DIR=$i/include/lua$PHP_LUA_VERSION
          AC_MSG_RESULT(found in a version-specific subdirectory of $i)
          break
        fi
      fi
    done
  fi

  if test -z "$LUA_INCLUDE_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please reinstall the lua distribution - lua.h should be in <lua-dir>/include/)
  fi

  if test "$PHP_LUA_VERSION" != "yes"; then
    LUA_LIB_SUFFIX=lua$PHP_LUA_VERSION
  else
    LUA_LIB_SUFFIX=lua$PHP_LUA_VERSION
  fi

  LUA_LIB_NAME=lib$LUA_LIB_SUFFIX

  if test -r $PHP_LUA/$PHP_LIBDIR/${LUA_LIB_NAME}.${SHLIB_SUFFIX_NAME} -o -r $PHP_LUA/$PHP_LIBDIR/${LUA_LIB_NAME}.a; then
    LUA_LIB_DIR=$PHP_LUA/$PHP_LIBDIR
  else
    AC_MSG_CHECKING(for lua library in default path)
    for i in /usr/$PHP_LIBDIR /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu; do
      if test -r $i/${LUA_LIB_NAME}.${SHLIB_SUFFIX_NAME} -o -r $i/${LUA_LIB_NAME}.a; then
        LUA_LIB_DIR=$i
        AC_MSG_RESULT(found in $i)
        break
      fi
    done
  fi

  if test -z "$LUA_LIB_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please reinstall the lua distribution - lua library should be in <lua-dir>/lib/)
  fi

  PHP_ADD_INCLUDE($LUA_INCLUDE_DIR)
  PHP_ADD_LIBRARY_WITH_PATH($LUA_LIB_SUFFIX, $LUA_LIB_DIR, LUA_SHARED_LIBADD) 
  PHP_SUBST(LUA_SHARED_LIBADD)
  PHP_NEW_EXTENSION(lua, lua.c lua_closure.c, $ext_shared)
fi
