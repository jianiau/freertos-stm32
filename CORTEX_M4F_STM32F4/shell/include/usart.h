#ifndef __USART_H
#define __USART_H


void init_rs232(void);

void enable_rs232_interrupts(void);

void enable_rs232(void);

void init_USART1(uint32_t baudrate);
#endif /* __USART_H */
