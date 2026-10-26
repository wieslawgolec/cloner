/*
 * Pure ANSI-C SHA-256 implementation.
 * No external dependencies. Works on Linux, FreeBSD, macOS, Cygwin, SunOS, etc.
 *
 * Provides SHA256Hash() with the same calling convention as the old MD5Hash()
 * so the validator logic can be swapped with minimal changes.
 */

#include <string.h>
#include "sha256.h"

/* SHA-256 constants */
static const unsigned long int K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static void SHA256Transform(unsigned long int state[8], unsigned char block[64])
{
    unsigned long int a, b, c, d, e, f, g, h, t1, t2, m[64];
    unsigned int i, j;

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = ((unsigned long int)block[j] << 24) |
               ((unsigned long int)block[j + 1] << 16) |
               ((unsigned long int)block[j + 2] << 8) |
               ((unsigned long int)block[j + 3]);

    for (; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void SHA256Init(struct SHA256_CTX *context)
{
    context->count[0] = context->count[1] = 0;
    context->state[0] = 0x6a09e667UL;
    context->state[1] = 0xbb67ae85UL;
    context->state[2] = 0x3c6ef372UL;
    context->state[3] = 0xa54ff53aUL;
    context->state[4] = 0x510e527fUL;
    context->state[5] = 0x9b05688cUL;
    context->state[6] = 0x1f83d9abUL;
    context->state[7] = 0x5be0cd19UL;
}

void SHA256Update(struct SHA256_CTX *context, unsigned char *input, unsigned long int inputLen)
{
    unsigned long int i, index, partLen;

    index = (unsigned long int)((context->count[0] >> 3) & 0x3F);

    if ((context->count[0] += (inputLen << 3)) < (inputLen << 3))
        context->count[1]++;
    context->count[1] += (inputLen >> 29);

    partLen = 64 - index;

    if (inputLen >= partLen) {
        memcpy(&context->buffer[index], input, partLen);
        SHA256Transform(context->state, context->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64)
            SHA256Transform(context->state, &input[i]);
        index = 0;
    } else {
        i = 0;
    }

    memcpy(&context->buffer[index], &input[i], inputLen - i);
}

void SHA256Final(unsigned char digest[32], struct SHA256_CTX *context)
{
    unsigned char bits[8];
    unsigned long int index, padLen;
    unsigned int i;

    for (i = 0; i < 4; ++i) {
        bits[i]     = (unsigned char)((context->count[1] >> (24 - i * 8)) & 0xff);
        bits[i + 4] = (unsigned char)((context->count[0] >> (24 - i * 8)) & 0xff);
    }

    index  = (unsigned long int)((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);

    {
        unsigned char padding[64];
        memset(padding, 0, 64);
        padding[0] = 0x80;
        SHA256Update(context, padding, padLen);
    }

    SHA256Update(context, bits, 8);

    for (i = 0; i < 8; ++i) {
        digest[i * 4]     = (unsigned char)((context->state[i] >> 24) & 0xff);
        digest[i * 4 + 1] = (unsigned char)((context->state[i] >> 16) & 0xff);
        digest[i * 4 + 2] = (unsigned char)((context->state[i] >> 8) & 0xff);
        digest[i * 4 + 3] = (unsigned char)(context->state[i] & 0xff);
    }

    memset(context, 0, sizeof(*context));
}

/*
 * Mirrors the original MD5Hash() behaviour:
 * - Process data in 64-byte blocks (zero-pad the last partial block)
 * - If keylen > 0, append one more 64-byte block containing the key (zero-padded)
 * - Produce a 32-byte SHA-256 digest
 */
void SHA256Hash(unsigned char digest[32], char *data, int datalen,
                unsigned char *key, int keylen)
{
    struct SHA256_CTX ctx;
    unsigned char p[64];

    if (keylen > 64)
        keylen = 64;

    SHA256Init(&ctx);

    while (datalen > 0) {
        if (datalen > 64) {
            memcpy(p, data, 64);
            datalen -= 64;
        } else {
            memset(p, 0, 64);
            memcpy(p, data, datalen);
            datalen = 0;
        }
        SHA256Update(&ctx, p, 64);
        data += 64;
    }

    if (keylen > 0) {
        memset(p, 0, 64);
        memcpy(p, key, keylen);
        SHA256Update(&ctx, p, 64);
    }

    SHA256Final(digest, &ctx);
}
