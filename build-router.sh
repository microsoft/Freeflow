cd libraries-router/librdmacm-1.1.0mlnx/
./autogen.sh
./configure --prefix=/usr/ --libdir=/usr/lib/ --sysconfdir=/etc/
make
make install

cd ../../ffrouter/
./build.sh
