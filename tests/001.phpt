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
try {
    $l->eval("invalid code");
} catch (LuaException $e) {
    assert($e->getCode() == LUA_ERRSYNTAX);
    echo "\n", $e->getMessage();
}

?>
--EXPECTF--
12
-0.53657291800043
[string "line"]:1: %s near 'code'
