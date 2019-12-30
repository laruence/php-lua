--TEST--
print with tables - 2
--SKIPIF--
<?php
if (!extension_loaded("lua")) print "skip lua extension missing";
?>
--FILE--
<?php
$lua = new Lua();
$lua->eval(<<<CODE
local a = {
  LEFT   = 1,
}
print(a)
CODE
);
?>
--EXPECT--
Array
 (
     [LEFT] => 1
 )
