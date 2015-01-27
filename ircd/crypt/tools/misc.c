/*
 * IRC - Internet Relay Chat, ircd/crypt/tea/cifranick.c
 * Copyright (C) 2002 IRC-Hispano.org - ESNET - zoltan
 *
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

/*
 * $Id: misc.c 324 2009-11-08 02:11:09Z dfmartinez $
 */

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
# include <winsock.h>
#else
# include <netinet/in.h>
#endif
#include <assert.h>
#include <ctype.h>
#include "misc.h"

#define NUMNICKLOG 6
#define NUMNICKMAXCHAR 'z'      /* See convert2n[] */
#define NUMNICKBASE 64          /* (2 << NUMNICKLOG) */
#define NUMNICKMASK 63          /* (NUMNICKBASE-1) */

#define SOCKIPLEN 45

/*
 * convert2y[] converts a numeric to the corresponding character.
 * The following characters are currently known to be forbidden:
 *
 * '\0' : Because we use '\0' as end of line.
 *
 * ' '  : Because parse_*() uses this as parameter seperator.
 * ':'  : Because parse_server() uses this to detect if a prefix is a
 *        numeric or a name.
 * '+'  : Because m_nick() uses this to determine if parv[6] is a
 *        umode or not.
 * '&', '#', '+', '$', '@' and '%' :
 *        Because m_message() matches these characters to detect special cases.
 */
static const char convert2y[] = {
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
  'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
  'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
  'w','x','y','z','0','1','2','3','4','5','6','7','8','9','[',']'
};

static const unsigned int convert2n[] = {
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  52,53,54,55,56,57,58,59,60,61, 0, 0, 0, 0, 0, 0,
   0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,25,62, 0,63, 0, 0,
   0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51, 0, 0, 0, 0, 0,

   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

unsigned int base64toint(const char *s)
{
  unsigned int i = convert2n[(unsigned char)*s++];
  while (*s)
  {
    i <<= NUMNICKLOG;
    i += convert2n[(unsigned char)*s++];
  }
  return i;
}

const char *inttobase64(char *buf, unsigned int v, unsigned int count)
{
  buf[count] = '\0';
  while (count > 0)
  {
    buf[--count] = convert2y[(v & NUMNICKMASK)];
    v >>= NUMNICKLOG;
  }
  return buf;
}

void buf_to_base64_r(unsigned char *out, const unsigned char *buf, size_t buf_len)
{
        size_t i, j;
        uint32_t limb;

/*        out = (unsigned char*) malloc(((buf_len * 8 + 5) / 6) + 5); */

        for (i = 0, j = 0, limb = 0; i + 2 < buf_len; i += 3, j += 4) {
                limb =
                        ((uint32_t) buf[i] << 16) |
                        ((uint32_t) buf[i + 1] << 8) |
                        ((uint32_t) buf[i + 2]);

                out[j] = convert2y[(limb >> 18) & 63];
                out[j + 1] = convert2y[(limb >> 12) & 63];
                out[j + 2] = convert2y[(limb >> 6) & 63];
                out[j + 3] = convert2y[(limb) & 63];
        }

        switch (buf_len - i) {
          case 0:
                break;
          case 1:
                limb = ((uint32_t) buf[i]);
                out[j++] = convert2y[(limb >> 2) & 63];
                out[j++] = convert2y[(limb << 4) & 63];
                out[j++] = '=';
                out[j++] = '=';
                break;
          case 2:
                limb = ((uint32_t) buf[i] << 8) | ((uint32_t) buf[i + 1]);
                out[j++] = convert2y[(limb >> 10) & 63];
                out[j++] = convert2y[(limb >> 4) & 63];
                out[j++] = convert2y[(limb << 2) & 63];
                out[j++] = '=';
                break;
          default:
                // something wonkey happened...
                break;
        }

        out[j] = '\0';
}

size_t base64_to_buf_r(unsigned char *buf, unsigned char *str)
{
        int i, j, len;
        uint32_t limb;
        size_t buf_len;

        len = strlen((char *) str);
        buf_len = (len * 6 + 7) / 8;
        /*buf = (unsigned char*) malloc(buf_len);*/
  
        for (i = 0, j = 0, limb = 0; i + 3 < len; i += 4) {
                if (str[i] == '=' || str[i + 1] == '=' || str[i + 2] == '=' || str[i + 3] == '=') {
                        if (str[i] == '=' || str[i + 1] == '=') {
                                break;
                        }
          
                        if (str[i + 2] == '=') {
                                limb =
                                        ((uint32_t) convert2n[str[i]] << 6) |
                                        ((uint32_t) convert2n[str[i + 1]]);
                                buf[j] = (unsigned char) (limb >> 4) & 0xff;
                                j++;
                        }
                        else {
                                limb =
                                        ((uint32_t) convert2n[str[i]] << 12) |
                                        ((uint32_t) convert2n[str[i + 1]] << 6) |
                                        ((uint32_t) convert2n[str[i + 2]]);
                                buf[j] = (unsigned char) (limb >> 10) & 0xff;
                                buf[j + 1] = (unsigned char) (limb >> 2) & 0xff;
                                j += 2;
                        }
                }
                else {
                        limb =
                                ((uint32_t) convert2n[str[i]] << 18) |
                                ((uint32_t) convert2n[str[i + 1]] << 12) |
                                ((uint32_t) convert2n[str[i + 2]] << 6) |
                                ((uint32_t) convert2n[str[i + 3]]);
          
                        buf[j] = (unsigned char) (limb >> 16) & 0xff;
                        buf[j + 1] = (unsigned char) (limb >> 8) & 0xff;
                        buf[j + 2] = (unsigned char) (limb) & 0xff;
                        j += 3;
                }
        }

        buf_len = j;
  
        return buf_len;
}

/** Encode an IP address in the base64 used by numnicks.
 * For IPv4 addresses (including IPv4-mapped and IPv4-compatible IPv6
 * addresses), the 32-bit host address is encoded directly as six
 * characters.
 *
 * For IPv6 addresses, each 16-bit address segment is encoded as three
 * characters, but the longest run of zero segments is encoded using an
 * underscore.
 * @param[out] buf Output buffer to write to.
 * @param[in] addr IP address to encode.
 * @param[in] count Number of bytes writable to \a buf.
 * @param[in] v6_ok If non-zero, peer understands base-64 encoded IPv6 addresses.
 */
const char* iptobase64(char* buf, const struct irc_in_addr* addr, unsigned int count, int v6_ok)
{
  if (irc_in_addr_is_ipv4(addr)) {
    assert(count >= 6);
    inttobase64(buf, (ntohs(addr->in6_16[6]) << 16) | ntohs(addr->in6_16[7]), 6);
  } else if (!v6_ok) {
    assert(count >= 6);
    if (addr->in6_16[0] == htons(0x2002))
        inttobase64(buf, (ntohs(addr->in6_16[1]) << 16) | ntohs(addr->in6_16[2]), 6);
    else
        strcpy(buf, "AAAAAA");
  } else {
    unsigned int max_start, max_zeros, curr_zeros, zero, ii;
    char *output = buf;

    assert(count >= 25);
    /* Can start by printing out the leading non-zero parts. */
    for (ii = 0; (addr->in6_16[ii]) && (ii < 8); ++ii) {
      inttobase64(output, ntohs(addr->in6_16[ii]), 3);
      output += 3;
    }
    /* Find the longest run of zeros. */
    for (max_start = zero = ii, max_zeros = curr_zeros = 0; ii < 8; ++ii) {
      if (!addr->in6_16[ii])
        curr_zeros++;
      else if (curr_zeros > max_zeros) {
        max_start = ii - curr_zeros;
        max_zeros = curr_zeros;
        curr_zeros = 0;
      }
    }
    if (curr_zeros > max_zeros) {
      max_start = ii - curr_zeros;
      max_zeros = curr_zeros;
      curr_zeros = 0;
    }
    /* Print the rest of the address */
    for (ii = zero; ii < 8; ) {
      if ((ii == max_start) && max_zeros) {
        *output++ = '_';
        ii += max_zeros;
      } else {
        inttobase64(output, ntohs(addr->in6_16[ii]), 3);
        output += 3;
        ii++;
      }
    }
    *output = '\0';
  }
  return buf;
}

/** Decode an IP address from base64.
 * @param[in] input Input buffer to decode.
 * @param[out] addr IP address structure to populate.
 */
void base64toip(const char* input, struct irc_in_addr* addr)
{
  memset(addr, 0, sizeof(*addr));
  if (strlen(input) == 6) {
    unsigned int in = base64toint(input);
    /* An all-zero address should stay that way. */
    if (in) {
      addr->in6_16[5] = htons(65535);
      addr->in6_16[6] = htons(in >> 16);
      addr->in6_16[7] = htons(in & 65535);
    }
  } else {
    unsigned int pos = 0;
    do {
      if (*input == '_') {
        unsigned int left;
        for (left = (25 - strlen(input)) / 3 - pos; left; left--)
          addr->in6_16[pos++] = 0;
        input++;
      } else {
        unsigned short accum = convert2n[(unsigned char)*input++];
        accum = (accum << NUMNICKLOG) | convert2n[(unsigned char)*input++];
        accum = (accum << NUMNICKLOG) | convert2n[(unsigned char)*input++];
        addr->in6_16[pos++] = ntohs(accum);
      }
    } while (pos < 8);
  }
}

/*
 * Funcion para obtener numeros aleatorios
 *
 * -- FreeMind
 */
unsigned int ircrandom() {
  unsigned int number;
  FILE* urandom = fopen("/dev/urandom", "r");
  if (urandom) {
    fread(&number, 1, sizeof(number), urandom);
    fclose(urandom);
  }
  return number;
}

void genera_aleatorio(unsigned char *out, int count) {
    int i;
    for(i=0;i<count;i++)
      *out++=(unsigned char) ircrandom();
}

/*
 * Funcion para transformar de ip a ascii reentrante
 */
void inetoa_r(char *buf, unsigned int ip)
{
  unsigned char *s = (unsigned char *)&ip;
  unsigned char a, b, c, d;

  a = *s++;
  b = *s++;
  c = *s++;
  d = *s++;
  sprintf(buf, "%u.%u.%u.%u", d, c, b, a);
}

/*
 * this new faster inet_ntoa was ripped from:
 * From: Thomas Helvey <tomh@inxpress.net>
 */
/** Array of text strings for dotted quads. */
static const char* IpQuadTab[] =
{
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",   "8",   "9",
   "10",  "11",  "12",  "13",  "14",  "15",  "16",  "17",  "18",  "19",
   "20",  "21",  "22",  "23",  "24",  "25",  "26",  "27",  "28",  "29",
   "30",  "31",  "32",  "33",  "34",  "35",  "36",  "37",  "38",  "39",
   "40",  "41",  "42",  "43",  "44",  "45",  "46",  "47",  "48",  "49",
   "50",  "51",  "52",  "53",  "54",  "55",  "56",  "57",  "58",  "59",
   "60",  "61",  "62",  "63",  "64",  "65",  "66",  "67",  "68",  "69",
   "70",  "71",  "72",  "73",  "74",  "75",  "76",  "77",  "78",  "79",
   "80",  "81",  "82",  "83",  "84",  "85",  "86",  "87",  "88",  "89",
   "90",  "91",  "92",  "93",  "94",  "95",  "96",  "97",  "98",  "99",
  "100", "101", "102", "103", "104", "105", "106", "107", "108", "109",
  "110", "111", "112", "113", "114", "115", "116", "117", "118", "119",
  "120", "121", "122", "123", "124", "125", "126", "127", "128", "129",
  "130", "131", "132", "133", "134", "135", "136", "137", "138", "139",
  "140", "141", "142", "143", "144", "145", "146", "147", "148", "149",
  "150", "151", "152", "153", "154", "155", "156", "157", "158", "159",
  "160", "161", "162", "163", "164", "165", "166", "167", "168", "169",
  "170", "171", "172", "173", "174", "175", "176", "177", "178", "179",
  "180", "181", "182", "183", "184", "185", "186", "187", "188", "189",
  "190", "191", "192", "193", "194", "195", "196", "197", "198", "199",
  "200", "201", "202", "203", "204", "205", "206", "207", "208", "209",
  "210", "211", "212", "213", "214", "215", "216", "217", "218", "219",
  "220", "221", "222", "223", "224", "225", "226", "227", "228", "229",
  "230", "231", "232", "233", "234", "235", "236", "237", "238", "239",
  "240", "241", "242", "243", "244", "245", "246", "247", "248", "249",
  "250", "251", "252", "253", "254", "255"
};

/** Convert an IP address to printable ASCII form.
 * This is generally deprecated in favor of ircd_ntoa_r().
 * @param[in] in Address to convert.
 * @return Pointer to a static buffer containing the readable form.
 */
const char* ircd_ntoa(const struct irc_in_addr* in)
{
  static char buf[SOCKIPLEN];
  return ircd_ntoa_r(buf, in);
}

/** Convert an IP address to printable ASCII form.
 * @param[out] buf Output buffer to write to.
 * @param[in] in Address to format.
 * @return Pointer to the output buffer \a buf.
 */
const char* ircd_ntoa_r(char* buf, const struct irc_in_addr* in)
{
    assert(buf != NULL);
    assert(in != NULL);

    if (irc_in_addr_is_ipv4(in)) {
      unsigned int pos, len;
      unsigned char *pch;

      pch = (unsigned char*)&in->in6_16[6];
      len = strlen(IpQuadTab[*pch]);
      memcpy(buf, IpQuadTab[*pch++], len);
      pos = len;
      buf[pos++] = '.';
      len = strlen(IpQuadTab[*pch]);
      memcpy(buf+pos, IpQuadTab[*pch++], len);
      pos += len;
      buf[pos++] = '.';
      len = strlen(IpQuadTab[*pch]);
      memcpy(buf+pos, IpQuadTab[*pch++], len);
      pos += len;
      buf[pos++] = '.';
      len = strlen(IpQuadTab[*pch]);
      memcpy(buf+pos, IpQuadTab[*pch++], len);
      buf[pos + len] = '\0';
      return buf;
    } else {
      static const char hexdigits[] = "0123456789abcdef";
      unsigned int pos, part, max_start, max_zeros, curr_zeros, ii;

      /* Find longest run of zeros. */
      for (max_start = ii = 1, max_zeros = curr_zeros = 0; ii < 8; ++ii) {
        if (!in->in6_16[ii])
          curr_zeros++;
        else if (curr_zeros > max_zeros) {
          max_start = ii - curr_zeros;
          max_zeros = curr_zeros;
          curr_zeros = 0;
        }
      }
      if (curr_zeros > max_zeros) {
        max_start = ii - curr_zeros;
        max_zeros = curr_zeros;
      }

      /* Print out address. */
/** Append \a CH to the output buffer. */
#define APPEND(CH) do { buf[pos++] = (CH); } while (0)
      for (pos = ii = 0; (ii < 8); ++ii) {
        if ((max_zeros > 0) && (ii == max_start)) {
          APPEND(':');
          ii += max_zeros - 1;
          continue;
        }
        part = ntohs(in->in6_16[ii]);
        if (part >= 0x1000)
          APPEND(hexdigits[part >> 12]);
        if (part >= 0x100)
          APPEND(hexdigits[(part >> 8) & 15]);
        if (part >= 0x10)
          APPEND(hexdigits[(part >> 4) & 15]);
        APPEND(hexdigits[part & 15]);
        if (ii < 7)
          APPEND(':');
      }
#undef APPEND

      /* Nul terminate and return number of characters used. */
      buf[pos++] = '\0';
      return buf;
    }
}

/** Attempt to parse an IPv4 address into a network-endian form.
 * @param[in] input Input string.
 * @param[out] output Network-endian representation of the address.
 * @param[out] pbits Number of bits found in pbits.
 * @return Number of characters used from \a input, or 0 if the parse failed.
 */
static unsigned int
ircd_aton_ip4(const char *input, unsigned int *output, unsigned char *pbits)
{
  unsigned int dots = 0, pos = 0, part = 0, ip = 0, bits;

  /* Intentionally no support for bizarre IPv4 formats (plain
   * integers, octal or hex components) -- only vanilla dotted
   * decimal quads.
   */
  if (input[0] == '.')
    return 0;
  bits = 32;
  while (1) switch (input[pos]) {
  case '\0':
    if (dots < 3)
      return 0;
  out:
    ip |= part << (24 - 8 * dots);
    *output = htonl(ip);
    if (pbits)
      *pbits = bits;
    return pos;
  case '.':
    if (input[++pos] == '.')
      return 0;
    ip |= part << (24 - 8 * dots++);
    part = 0;
    if (input[pos] == '*') {
      while (input[++pos] == '*' || input[pos] == '.') ;
      if (input[pos] != '\0')
        return 0;
      if (pbits)
        *pbits = dots * 8;
      *output = htonl(ip);
      return pos;
    }
    break;
  case '/':
    if (!pbits || !isdigit(input[pos + 1]))
      return 0;
    for (bits = 0; isdigit(input[++pos]); )
      bits = bits * 10 + input[pos] - '0';
    if (bits > 32)
      return 0;
    goto out;
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    part = part * 10 + input[pos++] - '0';
    if (part > 255)
      return 0;
    break;
  default:
    return 0;
  }
}

/** Parse a numeric IPv4 or IPv6 address into an irc_in_addr.
 * @param[in] input Input buffer.
 * @param[out] ip Receives parsed IP address.
 * @param[out] pbits If non-NULL, receives number of bits specified in address mask.
 * @return Number of characters used from \a input, or 0 if the
 * address was unparseable or malformed.
 */
int
ipmask_parse(const char *input, struct irc_in_addr *ip, unsigned char *pbits)
{
  char *colon;
  char *dot;

  assert(ip);
  assert(input);
  memset(ip, 0, sizeof(*ip));
  colon = strchr(input, ':');
  dot = strchr(input, '.');

  if (colon && (!dot || (dot > colon))) {
    unsigned int part = 0, pos = 0, ii = 0, colon = 8;
    const char *part_start = NULL;

    /* Parse IPv6, possibly like ::127.0.0.1.
     * This is pretty straightforward; the only trick is borrowed
     * from Paul Vixie (BIND): when it sees a "::" continue as if
     * it were a single ":", but note where it happened, and fill
     * with zeros afterward.
     */
    if (input[pos] == ':') {
      if ((input[pos+1] != ':') || (input[pos+2] == ':'))
        return 0;
      colon = 0;
      pos += 2;
      part_start = input + pos;
    }
    while (ii < 8) switch (input[pos]) {
      unsigned char chval;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      chval = input[pos] - '0';
    use_chval:
      part = (part << 4) | chval;
      if (part > 0xffff)
        return 0;
      pos++;
      break;
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
      chval = input[pos] - 'A' + 10;
      goto use_chval;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      chval = input[pos] - 'a' + 10;
      goto use_chval;
    case ':':
      part_start = input + ++pos;
      if (input[pos] == '.')
        return 0;
      ip->in6_16[ii++] = htons(part);
      part = 0;
      if (input[pos] == ':') {
        if (colon < 8)
          return 0;
        colon = ii;
        pos++;
      }
      break;
    case '.': {
      uint32_t ip4;
      unsigned int len;
      len = ircd_aton_ip4(part_start, &ip4, pbits);
      if (!len || (ii > 6))
        return 0;
      ip->in6_16[ii++] = htons(ntohl(ip4) >> 16);
      ip->in6_16[ii++] = htons(ntohl(ip4) & 65535);
      if (pbits)
        *pbits += 96;
      pos = part_start + len - input;
      goto finish;
    }
    case '/':
      if (!pbits || !isdigit(input[pos + 1]))
        return 0;
      ip->in6_16[ii++] = htons(part);
      for (part = 0; isdigit(input[++pos]); )
        part = part * 10 + input[pos] - '0';
      if (part > 128)
        return 0;
      *pbits = part;
      goto finish;
    case '*':
      while (input[++pos] == '*' || input[pos] == ':') ;
      if (input[pos] != '\0' || colon < 8)
        return 0;
      if (pbits)
        *pbits = ii * 16;
      return pos;
    case '\0':
      ip->in6_16[ii++] = htons(part);
      if (colon == 8 && ii < 8)
        return 0;
      if (pbits)
        *pbits = 128;
      goto finish;
    default:
      return 0;
    }
  finish:
    if (colon < 8) {
      unsigned int jj;
      /* Shift stuff after "::" up and fill middle with zeros. */
      for (jj = 0; jj < ii - colon; jj++)
        ip->in6_16[7 - jj] = ip->in6_16[ii - jj - 1];
      for (jj = 0; jj < 8 - ii; jj++)
        ip->in6_16[colon + jj] = 0;
    }
    return pos;
  } else if (dot || strchr(input, '/')) {
    unsigned int addr;
    int len = ircd_aton_ip4(input, &addr, pbits);
    if (len) {
      ip->in6_16[5] = htons(65535);
      ip->in6_16[6] = htons(ntohl(addr) >> 16);
      ip->in6_16[7] = htons(ntohl(addr) & 65535);
      if (pbits)
        *pbits += 96;
    }
    return len;
  } else if (input[0] == '*') {
    unsigned int pos = 0;
    while (input[++pos] == '*') ;
    if (input[pos] != '\0')
      return 0;
    if (pbits)
      *pbits = 0;
    return pos;
  } else return 0; /* parse failed */
}

/*
 * Funcion para hacer un XOR entre dos vectores
 *
 * Necesario para poder cifrar en CBC
 *
 */
void xor(uint8_t *dst, uint8_t *org, size_t tam) {
  unsigned int i=0;
  for(i=0;i<tam;i++)
    dst[i]=dst[i]^org[i];
}
