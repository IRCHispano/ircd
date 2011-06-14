#! /bin/sh

echo "IRC-Hispano IRCD"
echo ""
echo "Instalacion del demonio IRC para IRC-Hispano..."
echo "Para configurar un IRCD para los webchat HTML"
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

echo Configurando IRCD
cd ..
cd ..
./configure --prefix=$1 --with-maxcon=$2 --enable-p09 --enable-pcre --enable-webchathtml

echo ""
exit 0
