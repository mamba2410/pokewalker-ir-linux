#ifndef PW_COMPRESSION_H
#define PW_COMPRESSION_H

#include <stdint.h>
#include <stdlib.h>

void compress_data(uint8_t *data, uint8_t *buf, size_t dlen);
int decompress_data(uint8_t *data, uint8_t *buf, size_t dlen);

#endif /* PW_COMPRESSION_H */
