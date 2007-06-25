                    ARCHIVO DE INSTALACIÓN        by |[CaZoN]| <cazon@condonesdecolores.org>

================= AVISO - WARNING ================

Si modificas este fichero, no olvides modificar el fichero INSTALL que
se encuentra en el mismo directorio.

If you modify this file, do NOT forget to modify the INSTALL.es file in
the same directory.

==================================================

Este es el servidor de la red de IRC Hispano (basado en el servidor de IRC
Undernet).

La instalación del demonio de IRC (ircd) consiste en los siguientes pasos:


1) Descomprimir el archivo *.tar.
2) Entrar en el directorio base.
3 )`./configure'
4) `make config'
5) `make'
6) `make install'

1) Descomprimir el archivo *.tar
====================

El nombre del paquete es algo por el estilo a 'ircdx.y.z.tgz', donde
"x.y.z" equivale a versión actual (en el momento de escribir este texto la
versión actual es ircd2_10_07.tar.gz)

Necesitas el programa 'gzip', el descompresor de GNU, para descomprimir el
paquete.
Puedes descargarlo desde cualquier servidor de ftp GNU en casi cualquier
sistema operativo.

Si tienes el comando disponible tar de GNU, escribe:

tar xzf ircdx.y.z.tgz

En "ircdx.y.z.tgz" debemos poner el nombre del paquete.

Si tu versión no soporta el parámetro 'z', puedes escribirlo de forma
alternativa:

gzip -dc ircdx.y.z.tgz | tar xf -

Ambos métodos descomprimen el archivo en el directorio "ircdx.y.z/" del
directorio donde se ejecute el comando.

2) Entrar en el directorio base.
=============================

Entra en este directorio escribiendo en el directorio actual:

cd ircdx.y.z

Donde sustituyes "ircdx.y.z" por el nombre del directorio

3) `./configure'
==============

Este comando generará el archivo 'config/setup.h', la configuración
dependerá de tu sistema operativo.

4) `make config'
================

Este comando (re)generará eñ fichero 'include/config.h'. Puedes ejecutar
este comando tantas veces como quieras y usará los últimos valores como
predeterminados. En cada pregunta puedes escribir el símbolo '?' (seguido de
la tecla intro) para obtener una ayuda más extensa, ó también puedes
escribir 'c' para seguir usando los valores por defecto (finalizado rápido
del script).

5) `make'
=========

Escribe:

make

en el directorio base. Esto debería compilar sin errores o alarmas. Por
favor, envie un email con cualquier problema de mantenimiento, pero
solamente DESPUES de que estés seguro de que has hecho todo correctamente.
Si quieres que tu sistema operativo esté soportado en versiones futuras, lo
mejor que puedes hacer es crear un parche que solucione el problema.

6) `make install'
=================

Esto debería instalar el ircd y la página del manual (man page). Por favor,
comprueba los permisos del archivo binario.
Necesitas crear algunos de los archivos para loguear en el caso de que hayas
puesto sus nombres a mano antes de que el demonio de ircd empiece a
escribirlos. 
Necesitas corregir sintácticamente el archivo ircd.conf en el directorio
base. Lee algunos documentos para informarte sobre esto. Tambien es posible
crear el fichero ircd.motd con el texto de tu "Mensaje del Día" (message of
the day). Y finalmente, crear el fichero remote.motd con las tres lineas de
texto como mensaje del día remoto. Recordarles que todos estos ficheros
deben tener permisos para poder ser leidos por el demonio de irc, y los
archivos de log deben ser escribibles.

En caso de problemas
===================

Si tienes algún problema configurando el server, podrías considerar poner el
comando make de GNU en tu directorio. En algunos casos una shell /bin/sh en
mal funcionamiento puede ser el causante del problema. En tal caso te
sugiero instalar el intérprete de comandos "bash" y usar este (bash, en vez
de sh). Finalmente, cualquier otro problema de compilación deberá ser
resuelto cuando instales el gcc.

Si tienes problemas arrancando el demonio de irc, ejecuta 'make config' otra
vez y activa el DEBUGMODE. Recompila el demonio de irc, y arráncolo de esta
forma:

ircd -t -x9

Esto escribirá en la pantalla las salidad de depuración, y probablemente te mostrará
porque tu demonio no arranca.

No user el servidor con el DEBUGMODE definido para estar conectado a una
red.
