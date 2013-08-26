--TEST--
PHP Closures from Lua
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 

$l = new lua();
$l->eval(<<<CODE
function test(cb1)
    local cb2 = cb1("called from lua")
    cb2("returned from php")
end
CODE
);

try {
    $l->test(function($msg) {
        print "$msg";
    
        return function($msg) {
            print "$msg";
        };
    });
} catch(LuaException $e) {
    echo $e->getMessage();
}
?>
--EXPECTF--
called from luareturned from php
