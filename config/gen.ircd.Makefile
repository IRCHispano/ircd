. ./.config
. ./parse.none
mv ../ircd/Makefile ../ircd/Makefile.tmp
sed -e "s:^CC=.*:CC=$CC:" \
    -e "s:^CFLAGS=.*:CFLAGS=$CFLAGS:" \
    -e "s:^CPPFLAGS=.*:CPPFLAGS=$CPPFLAGS -I../zlib:" \
    -e "s:^LDFLAGS=.*:LDFLAGS=$LDFLAGS -L../zlib -lz:" \
    -e "s:^IRCDLIBS=.*:IRCDLIBS=$IRCDLIBS:" \
    -e "s:^IRCDMODE=.*:IRCDMODE=$IRCDMODE:" \
    -e "s:^IRCDOWN=.*:IRCDOWN=$IRCDOWN:" \
    -e "s:^IRCDGRP=.*:IRCDGRP=$IRCDGRP:" \
    -e "s:^IRC_UID=.*:IRC_UID=$IRC_UID:" \
    -e "s:^IRC_GID=.*:IRC_GID=$IRC_GID:" \
    -e "s:^BINDIR=.*:BINDIR=$BINDIR:" \
    -e "s:^SYMLINK=.*:SYMLINK=$SYMLINK:" \
    -e "s:^INCLUDEFLAGS=.*:INCLUDEFLAGS=$INCLUDEFLAGS -I../zlib:" \
    -e "s:^DPATH=.*:DPATH=$DPATH:" \
    -e "s:^MPATH=.*:MPATH=$MPATH:" \
    -e "s:^RPATH=.*:RPATH=$RPATH:" \
    -e "s:^DBPATH=.*:DBPATH=$DBPATH:" \
    -e "s:^BDD_MMAP=.*:BDD_MMAP=$BDD_MMAP:" \
    -e "s:^BDD_MMAP_PATH=.*:BDD_MMAP_PATH=$BDD_MMAP_PATH:" \
    -e "s:^INSTALL *= *\.\..*:INSTALL=../config/install-sh -c:" \
    ../ircd/Makefile.tmp > ../ircd/Makefile
$RM -f ../ircd/Makefile.tmp
