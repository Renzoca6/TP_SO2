#include <stdint.h>
#include <string.h>
#include <lib.h>
#include <moduleLoader.h>
#include <video.h>
#include <realTimeClock.h>
#include "interrupts_dispatcher.h"
#include "keyboard_handler.h"
#include "syscall.h"
#include "memory_manager.h"
#include "process.h"
#include "scheduler.h"
#include "interrupts.h"

// ---------------------------------------------------------------------
// Variables externas definidas en el linker
// ---------------------------------------------------------------------
extern uint8_t bss;
extern uint8_t endOfKernelBinary;
extern uint8_t endOfKernel;

// ---------------------------------------------------------------------
// Constantes
// ---------------------------------------------------------------------
static const uint64_t PageSize = 0x1000;
void *const shellAddress = (void *)0x400000;  // visible globalmente

#define HEAP_SIZE (4 * 1024 * 1024)   // 4 MB de heap

typedef int (*EntryPoint)();

// ---------------------------------------------------------------------
// Funciones auxiliares
// ---------------------------------------------------------------------
void clearBSS(void *bssAddress, uint64_t bssSize) {
    memset(bssAddress, 0, bssSize);
}

void *getStackBase(void) {
    return (void *)(
        (uint64_t)&endOfKernel +
        PageSize * 8 -              // Tamaño del stack (32 KiB)
        sizeof(uint64_t)            // Comienza en el tope del stack
    );
}

void *getHeapStart(void) {
    return (void *)((uint64_t)&endOfKernel + PageSize * 8);
}

// ---------------------------------------------------------------------
// Inicialización del kernel y carga de módulos
// ---------------------------------------------------------------------
void *initializeKernelBinary(void) {
    void *moduleAddresses[] = { shellAddress };

    loadModules(&endOfKernelBinary, moduleAddresses);
    clearBSS(&bss, &endOfKernel - &bss);

    return getStackBase();
}

// ---------------------------------------------------------------------
// Proceso idle — prioridad más baja, corre cuando no hay otro listo
// ---------------------------------------------------------------------
static void idle_process(void) {
    while (1) {
        _hlt();
    }
}

// ---------------------------------------------------------------------
// Punto de entrada principal
// ---------------------------------------------------------------------
int main(void) {
    mm_init(HEAP_START, HEAP_SIZE);
    init_interrupts();

    /* Inicializar subsistemas */
    mm_init(getHeapStart(), HEAP_SIZE);
    init_processes();
    init_scheduler();

    /* Crear procesos iniciales */
    create_process((void *)idle_process, "idle", PRIORITY_LEVELS - 1, 0);
    create_process((void *)shellAddress, "sh", 2, 1);

    /* El primer tick del timer hará el context switch al shell */
    while (1) {
        _hlt();
    }

    return 0;
}
