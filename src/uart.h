#include <stdbool.h>
#include <stdint.h>

extern void uart_init (void);
extern void uart_putc (const uint8_t c);
extern void uart_printf   (const char *restrict format, ...) __attribute__ ((format (printf, 1, 2)));
extern void uart_printf_P (const char *restrict format, ...) __attribute__ ((format (printf, 1, 2)));
extern bool uart_process (void);
extern const uint8_t *uart_line (void);
extern bool uart_flag_etx (void);
extern uint8_t uart_getchar (void);
