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

CC=gcc
CFLAGS=-W -Wall -g -ggdb

all: misc.o aes256.o tea.o cifranick ipreal ipvirtual cifraping descifraping cifratexto descifratexto

tea.o: tea.c tea.h
	$(CC) $(CFLAGS) -c -o tea.o tea.c

aes256.o: aes256.c aes256.h
	$(CC) $(CFLAGS) -c -o aes256.o aes256.c

misc.o: misc.c misc.h
	$(CC) $(CFLAGS) -c -o misc.o misc.c

cifraping: cifraping.c aes256.o misc.o
	$(CC) $(CFLAGS) -o cifraping cifraping.c aes256.o misc.o

descifraping: descifraping.c aes256.o misc.o
	$(CC) $(CFLAGS) -o descifraping descifraping.c aes256.o misc.o

cifratexto: cifratexto.c aes256.o misc.o
	$(CC) $(CFLAGS) -o cifratexto cifratexto.c aes256.o misc.o

descifratexto: descifratexto.c aes256.o misc.o
	$(CC) $(CFLAGS) -o descifratexto descifratexto.c aes256.o misc.o

cifranick: cifranick.c tea.o misc.o
	$(CC) $(CFLAGS) -o cifranick cifranick.c tea.o misc.o

ipreal: ipreal.c tea.o misc.o
	$(CC) $(CFLAGS) -o ipreal ipreal.c tea.o misc.o

ipvirtual: ipvirtual.c tea.o misc.o
	$(CC) $(CFLAGS) -o ipvirtual ipvirtual.c tea.o misc.o
	
install:       
	@echo 'Ya esta instalado, no hacia falta ;-).'

clean:
	/bin/rm -f cifranick ipreal ipvirtual core cifraping descifraping cifratexto descifratexto *.c~ *.o

distclean: clean
