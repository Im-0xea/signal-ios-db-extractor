#!/bin/sh

test -d ibranching || git clone https://github.com/Niki-Nu/ibranching.git
cd ibranching
make bootstrap
cp ib ../
cd ..


test -d libplist || git clone https://github.com/libimobiledevice/libplist.git
cd libplist
./autogen.sh
./configure --prefix=/usr
make
make install

wget https://www.fontsquirrel.com/fonts/download/inter
unzip inter
mv Inter-Regular.otf Inter.otf
