--TEST--
Lua return function
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 

$l = new lua();

$l->eval(<<<CODE
	function closure ()
       local from = "lua";
       return (function ()
          print(from);
       end);
	end
CODE
);

$func = $l->closure();

$l->call($func);
unset($func);
$func = $l->closure();

$l->call($func);
unset($func);
?>
--EXPECTF--
lualua
