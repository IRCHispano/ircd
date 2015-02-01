#! /bin/sh

echo "IRC-Hispano IRCD"
echo ""
echo "Instalacion del demonio IRC para IRC-Hispano..."
echo ""

if [ x"$2" = x ]
then
        echo "Uso $0 <prefijo ruta> <maximo de usuarios>"
        exit 1
fi

echo Configurando IRCD
./configure --prefix=$1 --with-maxcon=$2 --enable-pcre --with-undernet-environment

echo ""
exit 0
