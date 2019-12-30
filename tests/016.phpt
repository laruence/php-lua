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

local keys = {}

for k, _ in pairs(t) do
	table.insert(keys, k)
end

table.sort(keys)
for _,k in ipairs(keys) do
    print(k, " -> ", t[k], ",")
end
CODE
);

?>
--EXPECTF--
s -> string = 2,v -> 2,