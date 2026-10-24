struct MD5_CTX
{
  unsigned long int state[4];
  unsigned long int count[2];
  unsigned char buffer[64];
};


void MD5Init(struct MD5_CTX *context);
void MD5Update(struct MD5_CTX *context, unsigned char *input, unsigned long int inputLen);
void MD5Final(unsigned char digest[16], struct MD5_CTX *context);
void MD5Transform(unsigned long int state[4], unsigned char block[64]);
void Encode(unsigned char *output, unsigned long int *input, unsigned long int len);
void Decode(unsigned long int *output, unsigned char *input, unsigned long int len);

void MD5Hash(unsigned char digest[16], char *data, int datalen, unsigned char *key, int keylen);
void MD5HexHash(char digest[33], char *data, int datalen, unsigned char *key, int keylen);
void MD5CreateAuthString(char *str, int len);
int MD5HexValidate(char digest[33], char *data, int datalen, unsigned char *key, int keylen);
int MD5Validate(unsigned char digest[16], char *data, int datalen, unsigned char *key, int keylen);
int MD5HashFile(unsigned char digest[16], char *file, unsigned char *key, int keylen);
