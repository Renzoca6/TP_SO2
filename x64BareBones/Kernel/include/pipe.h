#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>

#define MAX_PIPES       16
#define PIPE_BUF_SIZE   4096
#define PIPE_NAME_LEN   32

typedef enum {
    PIPE_FREE = 0,
    PIPE_ACTIVE
} PipeState;

typedef struct {
    int       id;
    char      name[PIPE_NAME_LEN];
    PipeState state;
    char      buffer[PIPE_BUF_SIZE];
    uint32_t  read_pos;
    uint32_t  write_pos;
    uint32_t  count;        // bytes disponibles para leer
    int       write_open;   // extremos de escritura abiertos
    int       read_open;    // extremos de lectura abiertos
} Pipe;

void pipe_init(void);

// Crea un pipe anónimo. Retorna id o -1.
int  pipe_create(void);

// Crear o abrir pipe nombrado por `name`. Retorna pipe_id o -1 si no hay lugar.
// CONVENCIÓN OBLIGATORIA para procesos no relacionados:
//   - El proceso escritor debe llamar pipe_close(id, 0) para cerrar su extremo de lectura.
//   - El proceso lector debe llamar pipe_close(id, 1) para cerrar su extremo de escritura.
// Sin esta convención, el EOF nunca se detecta (write_open nunca llega a 0).
int  pipe_open(const char *name);

// Cierra un extremo del pipe (side: 0=lectura, 1=escritura).
int  pipe_close(int pipe_id, int side);

// Lectura bloqueante. Retorna bytes leídos, 0 en EOF, -1 en error.
int  pipe_read(int pipe_id, char *buf, uint32_t count);

// Escritura bloqueante. Retorna bytes escritos o -1 en error.
int  pipe_write(int pipe_id, const char *buf, uint32_t count);

// Despierta procesos bloqueados esperando este pipe.
void pipe_notify(int pipe_id);

#endif
