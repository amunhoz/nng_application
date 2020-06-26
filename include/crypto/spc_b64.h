#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Base64 encoding/decoding */
unsigned char *spc_base64_encode(unsigned char *input, size_t len, int wrap);
unsigned char *spc_base64_decode(unsigned char *buf, int size, size_t *len, int strict,
                                 int *err);
#ifdef __cplusplus
}
#endif
