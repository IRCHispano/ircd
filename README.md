NOTA IMPORTANTE
---------------

Este IRCD se ha dejado de desarrollar, los ultimos cambios (commit) son de transicion hacia el nuevo
IRCh, la ultima version va a ser obligatoria para hacer la migracion, intentar hacerla con versiones
antiguas puede producir efectos indeseables.

Para mas informacion, visite https://zolty.net

En los parches de transicion hay cambios en el formato de las tablas de las Bases de Datos Distribuidas,
tienen que estar migrados todos los registros a los formatos correctos para cada tabla para que pueda
funcionar correctamente en el nuevo IRCh.
Para más información de la Base de Datos Distribuidas, visite:

   https://zolty.net/ddb-bases-de-datos-distribuidas/


Repositorios del IRCD de IRC-Hispano
------------------------------------

El IRCD de IRC-Hispano se puede descargar en los siguientes repositorios de Git:

- Sourceforge: git://git.code.sf.net/p/irc-hispano/ircd
- GitHub: https://github.com/IRCHispano/ircd.git
- GitLab: https://gitlab.com/IRCHispano/ircd.git
- GitLab de IRC-Hispano (Privado): http://gitlab.chathispano.com/servicios/ircd.git

Los desarrollos se hacen contra el Gitlab y seran sincronizados con los
repositorios publicos para tener la ultima version del IRCD.
Actualmente, el GitLab privado de IRC-Hispano no esta actualizado por motivos tecnicos ajenos.


Este documento enumera las diferentes ramas del repositorio.


### RAMA OFICIAL

 * IRCD 2.10.H.10 Oficial

   IRCd utilizado en produccion en la red de IRC-Hispano.

   Etiqueta: u2_10_H_10




### RAMA DE DESARROLLO

 * IRCD 2.10.H.11

   IRCd de nueva generacion, con todas las ultimas tecnologias vigentes,
   desarrollo de nuevas caracteristicas y gestion de la base de datos.

   Etiqueta: master
   
   Esta rama se ha abandonado, ahora se usa su propio repositorio IRCh.



### Ejemplo

Ejemplo de descarga de la ultima version en produccion del IRCD de IRC-Hispano

`git clone --branch u2_10_H_10 git://git.code.sf.net/p/irc-hispano/ircd irc-hispano-ircd`
