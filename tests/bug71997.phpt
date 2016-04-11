--TEST--
Bug #71997 (One-Dimensional arrays cause segmentation faults)
--SKIPIF--
<?php
if (!extension_loaded("lua")) print "skip lua extension missing";
?>
--FILE--
<?php
$mylua = new \lua();
$mylua->eval(<<<CODE
    function nicefunction(args)
        print(args[1])
        return args[1]
    end
CODE
);

echo $mylua->call("nicefunction", array(array('hello', 'world')));
?>
done
--EXPECT--
worldworlddone
