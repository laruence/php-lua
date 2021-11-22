make clean
/usr/local/php-7.4.6/bin/phpize
./configure --with-php-config=/usr/local/php-7.4.6/bin/php-config --with-lua=/usr/local/lua-5.4.3/
make
make install
