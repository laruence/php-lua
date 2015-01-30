phpize

./configure  --with-lua-version=5.2
make
EX=$?;
if [ $EX != 0 ];
then
	echo "compile failed";
	exit $EX;
fi;



exit $EX;
