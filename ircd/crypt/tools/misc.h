/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef MISC_H_
#define MISC_H_

#define DUMP(s, i, buf, sz)  {printf(s);                   \
                              for (i = 0; i < (sz);i++)    \
                                  printf("%02x ", (unsigned char) buf[i]); \
                              printf("\n");}

/** Structure to store an IP address. */
struct irc_in_addr
{
  unsigned short in6_16[8]; /**< IPv6 encoded parts, little-endian. */
};

/** Evaluate to non-zero if \a ADDR (of type struct irc_in_addr) is an IPv4 address. */
#define irc_in_addr_is_ipv4(ADDR) (!(ADDR)->in6_16[0] && !(ADDR)->in6_16[1] && !(ADDR)->in6_16[2] \
                                   && !(ADDR)->in6_16[3] && !(ADDR)->in6_16[4] \
                                   && ((!(ADDR)->in6_16[5] && (ADDR)->in6_16[6]) \
                                       || (ADDR)->in6_16[5] == 65535))


/*
 * La siguiente tabla es utilizada por la macro toLower,
 * esta tabla esta extraida del archivo common.c del ircd.
 *
 * --Daijo
 */
#if 0
const char NTL_tolower_tab[] = {
       /* x00-x07 */ '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
       /* x08-x0f */ '\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
       /* x10-x17 */ '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
       /* x18-x1f */ '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',
       /* ' '-x27 */    ' ',    '!',    '"',    '#',    '$',    '%',    '&', '\x27',
       /* '('-'/' */    '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
       /* '0'-'7' */    '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
       /* '8'-'?' */    '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',
       /* '@'-'G' */    '@',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
       /* 'H'-'O' */    'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
       /* 'P'-'W' */    'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
       /* 'X'-'_' */    'x',    'y',    'z',    '{',    '|',    '}',    '~',    '_',
       /* '`'-'g' */    '`',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
       /* 'h'-'o' */    'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
       /* 'p'-'w' */    'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
       /* 'x'-x7f */    'x',    'y',    'z',    '{',    '|',    '}',    '~', '\x7f'
};
#define toLower(c) (NTL_tolower_tab[(c)])
#endif

extern void buf_to_base64_r(unsigned char *, const unsigned char *, size_t);
extern size_t base64_to_buf_r(unsigned char *, unsigned char *);
extern unsigned int ircrandom();
extern void genera_aleatorio(unsigned char *, int);
extern unsigned int base64toint(const char *str);
extern const char *inttobase64(char *buf, unsigned int v, unsigned int count);
extern const char* iptobase64(char* buf, const struct irc_in_addr* addr, unsigned int count, int v6_ok);
extern void base64toip(const char* s, struct irc_in_addr* addr);
extern void inetoa_r(char *buf, unsigned int ip);
extern const char* ircd_ntoa(const struct irc_in_addr* addr);
extern const char* ircd_ntoa_r(char* buf, const struct irc_in_addr* addr);
extern int ipmask_parse(const char *input, struct irc_in_addr *ip, unsigned char *pbits);
extern void xor(uint8_t *dst, uint8_t *org, size_t tam);

#endif /* MISC_H_ */
