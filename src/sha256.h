#ifndef SHA256_H
#define SHA256_H

struct SHA256_CTX {
    unsigned long int state[8];
    unsigned long int count[2];
    unsigned char buffer[64];
};

void SHA256Init(struct SHA256_CTX *context);
void SHA256Update(struct SHA256_CTX *context, unsigned char *input, unsigned long int inputLen);
void SHA256Final(unsigned char digest[32], struct SHA256_CTX *context);

/* Drop-in replacement for the old MD5Hash() interface.
 * digest must be at least 32 bytes.
 * Key is appended the same way the original MD5Hash did (simple keyed hash).
 */
void SHA256Hash(unsigned char digest[32], char *data, int datalen,
                unsigned char *key, int keylen);

#endif /* SHA256_H */
