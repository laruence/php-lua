--TEST--
Lua::include()
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
$filename = __FILE__.'.tmp';

$code = array(
  'fine'   => 'print "Hello PHP"',
  'broken' => 'dgdr fdrg erb rxdgre tews< df hxfdxgfc gsdgxvrsgrg.ve4w',
  'return' => 'return "php", "lua", "extension"', 
);

$l = new lua();

foreach ($code as $n => $c) {
    echo "\nTesting $n\n";
    file_put_contents($filename, $c);
    $ret = $l->include($filename);
	if ($ret) print_r($ret);
    @unlink($filename);
}
?>
--EXPECTF--

Testing fine
Hello PHP
Testing broken

Warning: Lua::include(): lua error: %s '=' expected near 'fdrg' in %s on line %d

Testing return
Array
(
    [0] => php
    [1] => lua
    [2] => extension
)
