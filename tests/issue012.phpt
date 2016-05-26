--TEST--
ISSUE #022 (Boolean FALSE is always TRUE)
--SKIPIF--
<?php
if (!extension_loaded("lua")) print "skip lua extension missing";
?>
--FILE--
<?php
$lua = new Lua();
$lua->assign('TEST', false);
$result = $lua->eval('return TEST');
var_dump($result)
?>
--EXPECT--
bool(false)
