--TEST--
PHP Object to lua
--SKIPIF--
<?php if (!extension_loaded("lua")) print "skip"; ?>
--FILE--
<?php 
class T {
    private $v;
    private $s;

    public function __construct($v)
    {
        $this->v = $v;
        $this->s = "string = $v";
    }

    static function create($arg)
    {
        return new self($arg);
    }
}


$l = new lua();
$l->registerCallback('create_object', [T::class, 'create']);

$l->eval(<<<CODE
local t = create_object(2)
table.sort( t, function( a, b ) return a > b end )
for i,k in pairs(t) do
    print(i, " -> ", k, ",")
end
CODE
);

?>
--EXPECTF--
v -> 2,s -> string = 2,