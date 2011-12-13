--TEST--
Bug (eval and include compute wrong return value number)
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

$arr = $l->eval(<<<CODE
	return 2, 3, 4, 5, 6
CODE
);

print_r($arr);

$l->call($func);
unset($func);
$func = $l->closure();

$l->call($func);
unset($func);
?>
--EXPECTF--
Array
(
    [0] => 2
    [1] => 3
    [2] => 4
    [3] => 5
    [4] => 6
)
lualua
