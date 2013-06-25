--TEST--
Basic lua check
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
$l = new lua();
$l->assign("b", 12);
$l->eval("print(b)");
$l->eval('print("\n")');
$l->eval("print(math.sin(b))");
$l->eval("invalid code");
--EXPECTF--
12
-0.53657291800043
Warning: Lua::eval(): lua error: [string "line"]:1: %s near 'code' in %s on line %d
