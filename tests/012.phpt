--TEST--
Check for index 0
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
$l = new lua();
$l->assign("b", range(1, 0));
?>
--EXPECTF--
Strict Standards: Lua::assign(): attempt to pass an array index begin with 0 to lua in %s012.php on line %d
