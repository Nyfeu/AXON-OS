#ifndef APPS_H
#define APPS_H

// Inicializa recursos das apps (mutexes, filas, etc)
void apps_init(void);

// Protótipos das Tarefas
void task_a(void);
void task_b(void);
void task_shell(void);

#endif