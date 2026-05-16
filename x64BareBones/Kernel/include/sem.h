#ifndef SEM_H
#define SEM_H

#include <stdint.h>

#define MAX_SEMS        32
#define MAX_SEM_NAME    32
#define MAX_BLOCKED     64

typedef struct {
    char     name[MAX_SEM_NAME];
    uint16_t value;
    uint8_t  lock;              // spinlock (semLock / semUnlock en ASM)
    uint16_t users;
    int      blocked_pids[MAX_BLOCKED];
    int      blocked_count;
    int      active;            // 1 si está en uso, 0 si libre
} Sem;

// Inicialización — llamar una vez desde kernel.c
void sem_init(void);

// Crear o abrir semáforo por nombre.
// Si ya existe incrementa users; value sólo se usa al crear.
// Retorna índice (>= 0) o -1 en error.
int  sem_open(const char *name, uint16_t value);

// Decrementar users. Si llega a 0, destruir el semáforo.
// Retorna 0 o -1 en error.
int  sem_close(int sem_id);

// Si value > 0: decrementar y retornar. Si value == 0: bloquear el proceso actual.
// Retorna 0 o -1 en error.
int  sem_wait(int sem_id);

// Si hay bloqueados: despertar el primero. Si no: incrementar value.
// Retorna 0 o -1 en error.
int  sem_post(int sem_id);

// Retorna value actual o -1 en error.
int  sem_get_value(int sem_id);

// Eliminar pid de la cola de bloqueados de todos los semáforos activos.
// Llamar desde kill_process() cuando un proceso muere bloqueado.
void sem_remove_pid(int pid);

#endif
