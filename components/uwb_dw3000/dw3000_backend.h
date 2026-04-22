#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*dw3000_spi_begin_fn_t)(void);
typedef uint8_t (*dw3000_spi_transfer_fn_t)(uint8_t data);
typedef void (*dw3000_spi_end_fn_t)(void);

void dw3000_set_spi_backend(dw3000_spi_begin_fn_t begin_fn,
                            dw3000_spi_transfer_fn_t transfer_fn,
                            dw3000_spi_end_fn_t end_fn);

bool dw3000_spi_backend_is_ready(void);
void dw3000_spi_backend_begin(void);
uint8_t dw3000_spi_backend_transfer(uint8_t data);
void dw3000_spi_backend_end(void);

#ifdef __cplusplus
}
#endif
