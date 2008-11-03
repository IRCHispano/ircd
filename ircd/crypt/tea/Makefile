#************************************************************************
#*   IRC - Internet Relay Chat, ircd/crypt/tea/Makefile
#*   Copyright (C) 2002 IRC-Hispano.org - ESNET - zoltan
#*
#*   This program is free software; you can redistribute it and/or modify
#*   it under the terms of the GNU General Public License as published by
#*   the Free Software Foundation; either version 1, or (at your option)
#*   any later version.
#*
#*   This program is distributed in the hope that it will be useful,
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*   GNU General Public License for more details.
#*
#*   You should have received a copy of the GNU General Public License
#*   along with this program; if not, write to the Free Software
#*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#*/
#
# Makefile para compilar el programa de encriptar las claves
# de los nicks para la tabla 'n'.



all: cifranick ipreal ipvirtual

cifranick: cifranick.c
	gcc -Wall -O2 cifranick.c -o cifranick

ipreal: ipreal.c
	gcc -Wall -O2 ipreal.c -o ipreal

ipvirtual: ipvirtual.c
	gcc -Wall -O2 ipvirtual.c -o ipvirtual

install:       
	@echo 'Ya esta instalado, no hacia falta ;-).'

clean:
	/bin/rm -f cifranick ipreal ipvirtual core *.c~

distclean: clean
