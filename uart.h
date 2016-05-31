#define UART_FIFOSIZE	32

struct uart_fifo {
	uint8_t fifo[UART_FIFOSIZE];
	uint8_t tail;
	uint8_t head;
};

void uart_init (void);
void uart_putc (const uint8_t c);
void uart_printf   (const char *restrict format, ...) __attribute__ ((format (printf, 1, 2)));
void uart_printf_P (const char *restrict format, ...) __attribute__ ((format (printf, 1, 2)));
bool uart_process (void);
const uint8_t *uart_line (void);

// Rx FIFO is global:
extern struct uart_fifo rx;
