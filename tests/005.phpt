--TEST--
Lua phpinfo() block 
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
$r = new ReflectionExtension('lua');
$r->info();
?>
--EXPECTF--
lua

lua support => enabled
lua extension version => %s
lua release => %s
lua copyright => %s
lua authors => %s
