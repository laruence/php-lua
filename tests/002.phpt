--TEST--
Set and read properties
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
$l = new lua();
$l->a = "foobar";
$l->b = 12;
var_dump($l->b);
var_dump($l->a);

$l->b = null;
var_dump($l->b);
?>
--EXPECT--
float(12)
string(6) "foobar"
NULL
