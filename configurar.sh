#! /bin/sh

echo "IRC-Hispano IRCD"
echo ""
echo "Instalacion del demonio IRC..."
echo ""

if [ x"$2" = x ]
then
        echo "Uso $0 <prefijo ruta> <maximo de usuarios>"
        exit 1
fi

echo
echo
echo Configurando libreria ZLIB...
cd libs
cd zlib
./configure
make -f Makefile2

echo Configurando libreria libevent...
cd ..
cd libevent
./configure --disable-shared --enable-static
make build

echo Configurando IRCD
cd ..
cd ..
./configure --prefix=$1 --with-maxcon=$2

echo ""
exit 0
