/*  
*   Byte-oriented AES-256 implementation.
*   All lookup tables replaced with 'on the fly' calculations. 
*
*   Copyright (c) 2007 Ilya O. Levin, http://www.literatecode.com
*
*   Permission to use, copy, modify, and distribute this software for any
*   purpose with or without fee is hereby granted, provided that the above
*   copyright notice and this permission notice appear in all copies.
*
*   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
*   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
*   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
*   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
*   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
*   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
*   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * $Id$
 */

#include "aes256.h"

#define F(x)   (((x)<<1) ^ ((((x)>>7) & 1) * 0x1b))
#define FD(x)  (((x) >> 1) ^ (((x) & 1) ? 0x8d : 0))

/* -------------------------------------------------------------------------- */
uint8_t gf_alog(uint8_t x) // calculate anti-logarithm gen 3
{
    uint8_t atb = 1, z;

    while (x--) {z = atb; atb <<= 1; if (z & 0x80) atb^= 0x1b; atb ^= z;}

    return atb;
} /* gf_alog */

/* -------------------------------------------------------------------------- */
uint8_t gf_log(uint8_t x) // calculate logarithm gen 3
{
    uint8_t atb = 1, i = 0, z;

    do {
        if (atb == x) break;
        z = atb; atb <<= 1; if (z & 0x80) atb^= 0x1b; atb ^= z;
    } while (++i > 0);

    return i;
} /* gf_log */


/* -------------------------------------------------------------------------- */
uint8_t gf_mulinv(uint8_t x) // calculate multiplicative inverse
{
    return (x) ? gf_alog(255 - gf_log(x)) : 0;
} /* gf_mulinv */

/* -------------------------------------------------------------------------- */
uint8_t rj_sbox(uint8_t x)
{
    uint8_t y, sb;

    sb = y = gf_mulinv(x);
    y = (y<<1)|(y>>7); sb ^= y;  y = (y<<1)|(y>>7); sb ^= y; 
    y = (y<<1)|(y>>7); sb ^= y;  y = (y<<1)|(y>>7); sb ^= y;

    return (sb ^ 0x63);
} /* rj_sbox */

/* -------------------------------------------------------------------------- */
uint8_t rj_xtime(uint8_t x) 
{
    return (x & 0x80) ? ((x << 1) ^ 0x1b) : (x << 1);
} /* rj_xtime */


/* -------------------------------------------------------------------------- */
void aes_subBytes(uint8_t *buf)
{
    register uint8_t i = 16;
    while (i--) buf[i] = rj_sbox(buf[i]);
} /* aes_subBytes */

/* -------------------------------------------------------------------------- */
void aes_subBytes_inv(uint8_t *buf)
{
    register uint8_t i = 16, j;
    while (i--) 
    {
        j = 0; do { if (rj_sbox(j) == buf[i]) {buf[i] = j; break;} } while (++j);
    }
} /* aes_subBytes_inv */

/* -------------------------------------------------------------------------- */
void aes_addRoundKey(uint8_t *buf, uint8_t *key)
{
    register uint8_t i = 16;
    while (i--) buf[i] ^= key[i];
} /* aes_addRoundKey */

/* -------------------------------------------------------------------------- */
void aes_addRoundKey_cpy(uint8_t *buf, uint8_t *key, uint8_t *cpk)
{
    register uint8_t i = 16;
    while (i--)  buf[i] ^= (cpk[i] = key[i]), cpk[16+i] = key[16 + i];
} /* aes_addRoundKey_cpy */


/* -------------------------------------------------------------------------- */
void aes_shiftRows(uint8_t *buf)
{
    register uint8_t i, j; /* to make it potentially parallelable :) */

    i = buf[1]; buf[1] = buf[5]; buf[5] = buf[9]; buf[9] = buf[13]; buf[13] = i;
    i = buf[10]; buf[10] = buf[2]; buf[2] = i;
    j = buf[3]; buf[3] = buf[15]; buf[15] = buf[11]; buf[11] = buf[7]; buf[7] = j;
    j = buf[14]; buf[14] = buf[6]; buf[6]  = j;

} /* aes_shiftRows */

/* -------------------------------------------------------------------------- */
void aes_shiftRows_inv(uint8_t *buf)
{
    register uint8_t i, j; /* same as above :) */

    i = buf[1]; buf[1] = buf[13]; buf[13] = buf[9]; buf[9] = buf[5]; buf[5] = i;
    i = buf[2]; buf[2] = buf[10]; buf[10] = i;
    j = buf[3]; buf[3] = buf[7]; buf[7] = buf[11]; buf[11] = buf[15]; buf[15] = j;
    j = buf[6]; buf[6] = buf[14]; buf[14] = j;

} /* aes_shiftRows_inv */

/* -------------------------------------------------------------------------- */
void aes_mixColumns(uint8_t *buf)
{
    register uint8_t i, a, b, c, d, e;

    for (i = 0; i < 16; i += 4)
    {
        a = buf[i]; b = buf[i + 1]; c = buf[i + 2]; d = buf[i + 3];
        e = a ^ b ^ c ^ d;
        buf[i] ^= e ^ rj_xtime(a^b);   buf[i+1] ^= e ^ rj_xtime(b^c);
        buf[i+2] ^= e ^ rj_xtime(c^d); buf[i+3] ^= e ^ rj_xtime(d^a);
    }
} /* aes_mixColumns */

/* -------------------------------------------------------------------------- */
void aes_mixColumns_inv(uint8_t *buf)
{
    register uint8_t i, a, b, c, d, e, x, y, z;

    for (i = 0; i < 16; i += 4)
    {
        a = buf[i]; b = buf[i + 1]; c = buf[i + 2]; d = buf[i + 3];
        e = a ^ b ^ c ^ d;
        z = rj_xtime(e);
        x = e ^ rj_xtime(rj_xtime(z^a^c));  y = e ^ rj_xtime(rj_xtime(z^b^d));
        buf[i] ^= x ^ rj_xtime(a^b);   buf[i+1] ^= y ^ rj_xtime(b^c);
        buf[i+2] ^= x ^ rj_xtime(c^d); buf[i+3] ^= y ^ rj_xtime(d^a);
    }
} /* aes_mixColumns_inv */

/* -------------------------------------------------------------------------- */
void aes_expandEncKey(uint8_t *k, uint8_t *rc) 
{
    register uint8_t i;

    k[0] ^= rj_sbox(k[29]) ^ (*rc);
    k[1] ^= rj_sbox(k[30]);
    k[2] ^= rj_sbox(k[31]);
    k[3] ^= rj_sbox(k[28]);
    *rc = F( *rc);

    for(i = 4; i < 16; i += 4)  k[i] ^= k[i-4],   k[i+1] ^= k[i-3],
                                k[i+2] ^= k[i-2], k[i+3] ^= k[i-1];
    k[16] ^= rj_sbox(k[12]);
    k[17] ^= rj_sbox(k[13]);
    k[18] ^= rj_sbox(k[14]);
    k[19] ^= rj_sbox(k[15]);

    for(i = 20; i < 32; i += 4) k[i] ^= k[i-4],   k[i+1] ^= k[i-3],
                                k[i+2] ^= k[i-2], k[i+3] ^= k[i-1];

} /* aes_expandEncKey */

/* -------------------------------------------------------------------------- */
void aes_expandDecKey(uint8_t *k, uint8_t *rc) 
{
    uint8_t i;

    for(i = 28; i > 16; i -= 4) k[i+0] ^= k[i-4], k[i+1] ^= k[i-3], 
                                k[i+2] ^= k[i-2], k[i+3] ^= k[i-1];

    k[16] ^= rj_sbox(k[12]);
    k[17] ^= rj_sbox(k[13]);
    k[18] ^= rj_sbox(k[14]);
    k[19] ^= rj_sbox(k[15]);

    for(i = 12; i > 0; i -= 4)  k[i+0] ^= k[i-4], k[i+1] ^= k[i-3],
                                k[i+2] ^= k[i-2], k[i+3] ^= k[i-1];

    *rc = FD(*rc);
    k[0] ^= rj_sbox(k[29]) ^ (*rc);
    k[1] ^= rj_sbox(k[30]);
    k[2] ^= rj_sbox(k[31]);
    k[3] ^= rj_sbox(k[28]);
} /* aes_expandDecKey */


/* -------------------------------------------------------------------------- */
void aes256_init(aes256_context *ctx, uint8_t *k)
{
    uint8_t rcon = 1;
    register uint8_t i;
    for (i = 0; i < sizeof(ctx->key); i++) ctx->enckey[i] = ctx->deckey[i] = k[i];
    for (i = 8;--i;) aes_expandEncKey(ctx->deckey, &rcon);
} /* aes256_init */

/* -------------------------------------------------------------------------- */
void aes256_done(aes256_context *ctx)
{
    register uint8_t i;
    for (i = 0; i < sizeof(ctx->key); i++) 
        ctx->key[i] = ctx->enckey[i] = ctx->deckey[i] = 0;
} /* aes256_done */

/* -------------------------------------------------------------------------- */
void aes256_encrypt_ecb(aes256_context *ctx, uint8_t *buf)
{
    uint8_t i, rcon;

    aes_addRoundKey_cpy(buf, ctx->enckey, ctx->key);
    for(i = 1, rcon = 1; i < 14; ++i)
    {
        aes_subBytes(buf);
        aes_shiftRows(buf);
        aes_mixColumns(buf);
        if( i & 1 ) aes_addRoundKey( buf, &ctx->key[16]);
        else aes_expandEncKey(ctx->key, &rcon), aes_addRoundKey(buf, ctx->key);
    }
    aes_subBytes(buf);
    aes_shiftRows(buf);
    aes_expandEncKey(ctx->key, &rcon); 
    aes_addRoundKey(buf, ctx->key);
} /* aes256_encrypt */

/* -------------------------------------------------------------------------- */
void aes256_decrypt_ecb(aes256_context *ctx, uint8_t *buf)
{
    uint8_t i, rcon;

    aes_addRoundKey_cpy(buf, ctx->deckey, ctx->key);
    aes_shiftRows_inv(buf);
    aes_subBytes_inv(buf);

    for (i = 14, rcon = 0x80; --i;)
    {
        if( ( i & 1 ) )           
        {
            aes_expandDecKey(ctx->key, &rcon);
            aes_addRoundKey(buf, &ctx->key[16]);
        }
        else aes_addRoundKey(buf, ctx->key);
        aes_mixColumns_inv(buf);
        aes_shiftRows_inv(buf);
        aes_subBytes_inv(buf);
    }
    aes_addRoundKey( buf, ctx->key); 
} /* aes256_decrypt */
