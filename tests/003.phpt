--TEST--
Call lua functions
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
$l = new lua();
$l->eval(<<<CODE
function phptest_no_param()
    print("No parameters!\\n")
end

function phptest_1_param(param)
    print("Parameter: `", param, "'!\\n")
end

function phptest_2_params(param1, param2)
    print("Parameter 1: `", param1, "', Parameter 2: `", param2, "'!\\n")
end

function multiret()
    return "a", "b", "c"
end
CODE
);

$l->print("Hello world!\n");
$l->phptest_no_param();

$l->phptest_1_param();
$l->phptest_1_param("foobar");

$l->phptest_2_params();
$l->phptest_2_params("foo");
$l->phptest_2_params("foo", "bar");

echo "\n";

var_dump($l->call(array("math", "sin"), array(3/2*pi())));
var_dump($l->call(array("math", "cos"),  array(pi())));

echo "\n";

try {
$l->notexisting(2423);
$l->call(array("math", "sin", "function"), array(432, 342));

} catch (LuaException $e) {
	echo $e->getMessage();
}

echo "\n";

var_dump($l->type("fobar"));
var_dump($l->multiret());

echo "\n";

var_dump($l->__call("multiret", array()));

$l->__call("print");
$l->__call("print", array("foo"));
?>
--EXPECTF--
Hello world!
No parameters!
Parameter: `'!
Parameter: `foobar'!
Parameter 1: `', Parameter 2: `'!
Parameter 1: `foo', Parameter 2: `'!
Parameter 1: `foo', Parameter 2: `bar'!

float(-1)
float(-1)

invalid lua function 'notexisting'
string(6) "string"
array(3) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
  [2]=>
  string(1) "c"
}

array(3) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
  [2]=>
  string(1) "c"
}
foo
