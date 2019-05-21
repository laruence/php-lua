--TEST--
ISSUE #040 (segment fault)
--SKIPIF--
<?php
if (!extension_loaded("lua")) print "skip lua extension missing";
?>
--FILE--
<?php
$lua = new Lua();
$lua->eval(<<<CODE
local a = {}
print(a)
CODE
);
?>
--EXPECT--
Array
 (
 )