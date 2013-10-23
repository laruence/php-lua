--TEST--
Lua::include() with error codes
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
$filename = __FILE__.'.tmp';

$l = new lua();
try {
    $ret = $l->include($filename);
} catch (LuaException $e) {
    assert($e->getCode() == LUA_ERRFILE);
    echo "\n".$e->getMessage();
}
?>
--EXPECTF--
cannot open %s012.php.tmp: No such file or directory
