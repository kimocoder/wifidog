#ifndef __STRINGS_H__
#define __STRINGS_H__

bool isasciistring(int len, uint8_t *buffer);
bool hex2bin(const char *str, uint8_t *bytes, size_t blen);

#endif // __STRINGS_H__

