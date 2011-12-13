--TEST--
LuaClosure exception
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

try {
	$l->call($func);
    if (version_compare(PHP_VERSION, "5.3.0") >= 0) {
		$func();
	} else {
		$func->invoke();
	}	
} catch (LuaException $e) {
	echo $e->getMessage();
}
?>
--EXPECTF--
lualua
