--TEST--
Type conversion from lua to PHP
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
$l = new lua();

$l->eval(<<<CODE
a = nil
b = false
c = 23
d = -432
e = 432.432
f = 0
g = 14.3e24
h = "foobar"
i = ""
CODE
);

for ($i = 'a'; $i < 'j'; $i++) {
    var_dump($l->{$i});
}
?>
--EXPECT--
NULL
bool(false)
float(23)
float(-432)
float(432.432)
float(0)
float(1.43E+25)
string(6) "foobar"
string(0) ""
