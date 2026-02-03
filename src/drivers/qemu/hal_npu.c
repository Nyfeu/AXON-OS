#include "../../../include/hal/hal_npu.h"
#include "../../../include/hal/hal_uart.h"

// Stubs para enganar o linker no QEMU
void hal_npu_init(void) {}
void hal_npu_configure(uint32_t k, npu_quant_params_t *q) {}
void hal_npu_load_weights(const uint32_t *d, uint32_t n) {}
void hal_npu_load_inputs(const uint32_t *d, uint32_t n) {}
void hal_npu_read_output(uint32_t *b, uint32_t n) {}
void hal_npu_start(void) { hal_uart_puts("[SIM] NPU Start (Ignored)\n\r"); }
int hal_npu_is_busy(void) { return 0; } // Sempre livre
void hal_npu_wait_done(void) {}
void hal_npu_set_dma_enabled(bool e) {}
void hal_npu_start_accumulate(void) {}
void hal_npu_load_bias(const uint32_t *b) {}