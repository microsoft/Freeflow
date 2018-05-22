cd libraries-router/libmlx4-1.2.1mlnx1/
./autogen.sh
./configure --prefix=/usr/ --libdir=/usr/lib/ --sysconfdir=/etc/
make
make install

cd ../libibverbs-1.2.1mlnx1/
./autogen.sh
./configure --prefix=/usr/ --libdir=/usr/lib/ --sysconfdir=/etc/
make
make install

cd ../librdmacm-1.1.0mlnx/
./autogen.sh
./configure --prefix=/usr/ --libdir=/usr/lib/ --sysconfdir=/etc/
make
make install

cd ../../ffrouter/
./build.sh
