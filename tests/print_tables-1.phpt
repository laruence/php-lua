--TEST--
print with tables - 1
--SKIPIF--
<?php
if (!extension_loaded("lua")) print "skip lua extension missing";
?>
--FILE--
<?php
$lua = new Lua();
$lua->eval(<<<CODE
local people = {
   {
      phone = "123456"
   },
}
print(people)
CODE
);
?>
--EXPECT--
Array
 (
     [1] => Array
         (
             [phone] => 123456
         )

 )
