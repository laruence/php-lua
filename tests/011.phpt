--TEST--
register invalid php callback to lua
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 

$l = new lua();
    
try {
	$l->registerCallback("callphp", "print"); 
	echo "okey";
} catch (Exception $e) {
	echo  $e->getMessage();
}

--EXPECT--
invalid php callback
