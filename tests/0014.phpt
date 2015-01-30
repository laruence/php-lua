--TEST--
Check for Table Pass vic-a-verse
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
$l = new lua();
$l->eval(<<<CODE
function test(a)
    lua_fcn(a)
end
CODE
);

$l->registerCallback("lua_fcn", function($a) {
    ksort($a);
    var_dump($a);
});
$l->test(array("key1"=>"v1",
                "key2"=>"v2"));



--EXPECT--
array(2) {
  ["key1"]=>
  string(2) "v1"
  ["key2"]=>
  string(2) "v2"
}