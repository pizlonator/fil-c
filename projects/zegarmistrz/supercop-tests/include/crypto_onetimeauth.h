#ifndef crypto_onetimeauth_h
#define crypto_onetimeauth_h
extern int crypto_onetimeauth(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *);
extern int crypto_onetimeauth_verify(const unsigned char *,const unsigned char *,unsigned long long,const unsigned char *);
#endif
