--TEST--
Bug #65097 (nApplyCount release missing)
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--INI--
error_reporting=E_ALL&~E_STRICT
--FILE--
<?php
$lua = new Lua();
$lua->eval("
function main(x)
 return nil
end");
$obj = array(1, 2, 3, array(4, 5, 6));
echo json_encode($obj) . "\n";
$lua->call('main', array($obj));
echo json_encode($obj) . "\n";
?>
--EXPECT--
[1,2,3,[4,5,6]]
[1,2,3,[4,5,6]]
