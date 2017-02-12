--TEST--
Bug #73964 (Segmentation fault (11))
--SKIPIF--
<?php
if (!extension_loaded("lua")) print "skip lua extension missing";
?>
--FILE--
<?php

class LuaTest
{
	function __construct()
	{ 
		$this->lua = new Lua();
		$this->lua->registerCallback("log", array($this, "API_LuaLog"));   
	}


	function API_LuaLog( $entry)
	{
		echo("from lua: $entry");
	}   

	public function RunTest()
	{
		$this->lua->eval(<<<CODE
function TestFunc(str)
	 log(str)

end

CODE
	);               
		$GamePackage = $this->lua->RootTable;

		$this->lua->call("TestFunc", array("okey"));
	}
}


$a = new LuaTest();
$a->runtest();
?>
--EXPECT--
from lua: okey
