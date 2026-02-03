#ifndef APPS_H
#define APPS_H

// Inicializa recursos das apps (mutexes, filas, etc)
void apps_init(void);

// Protótipos das Tarefas
void task_shell(void);
void task_leds(void);
void task_monitor(void);

#endif