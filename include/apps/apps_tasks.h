#ifndef APPS_TASKS_H
#define APPS_TASKS_H

// Protótipos das tarefas 
void task_shell(void);
void task_leds(void);
void task_monitor(void);

// ISR para UART
void uart_isr(void);

#endif
