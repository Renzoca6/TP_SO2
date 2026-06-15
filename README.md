# TP2 — Núcleo de Sistema Operativo

**Materia:** Sistemas Operativos (72.11) — ITBA  
**Trabajo Práctico:** TP2 

---

## Estructura del repositorio

```
TP_SO2/
├── README.md
└── x64BareBones/
    ├── Bootloader/      # Cargador de arranque (stage 1 y 2)
    ├── Image/           # Generación de la imagen de disco QCOW2
    ├── Kernel/          # Núcleo: scheduler, procesos, memoria, semáforos, pipes
    ├── Toolchain/       # Cross-compiler y herramientas de enlazado
    ├── Userland/        # Shell y aplicaciones de usuario
    ├── compile.sh       # Script de compilación (vía Docker)
    ├── run.sh           # Script de ejecución (QEMU)
    └── Makefile         # Makefile raíz del proyecto
```

---

## Compilación y ejecución

### Requisitos previos

- [Docker](https://www.docker.com/) instalado y en ejecución.
- Imagen de la cátedra disponible localmente: `agodio/itba-so-multiarch:3.1`.
- `qemu-system-x86_64` instalado en el host.

### 1. Crear el contenedor (una sola vez)

Ejecutar **desde el directorio `x64BareBones/`**:

```bash
docker run --name tp-so -v "${PWD}:/root" --privileged -d \
    agodio/itba-so-multiarch:3.1 tail -f /dev/null
```

Esto monta `x64BareBones/` como `/root` dentro del contenedor. El contenedor puede
quedar detenido entre compilaciones; `compile.sh` lo inicia automáticamente con
`docker start`.

### 2. Compilar (Buddy Allocator — default)

Desde `x64BareBones/`:

```bash
./compile.sh
```

El script realiza en orden:

1. Inicia el contenedor (`docker start tp-so`).
2. `make clean` en Toolchain y en la raíz del proyecto.
3. `make` en Toolchain (cross-compiler).
4. `make` en la raíz del proyecto — compila el kernel con el **Buddy System** por defecto.

> Para usar un nombre de contenedor distinto: `./compile.sh <nombre>` (default: `tp-so`).

### 3. Compilar con Bitmap Memory Manager

El `Makefile` raíz no expone un target `bitmap`; dicho target existe en
`Kernel/Makefile`. Después de una compilación normal, para cambiar al allocator de
bitmap ejecutar:

```bash
docker exec -it tp-so bash -c \
    "make clean -C /root/Kernel && make bitmap -C /root/Kernel && make -C /root/Image"
```

Para volver al Buddy System basta con ejecutar `./compile.sh` de nuevo.

### 4. Ejecutar

Desde `x64BareBones/`:

```bash
./run.sh
```

Lanza `qemu-system-x86_64` con la imagen `Image/x64BareBonesImage.qcow2` y 512 MB de
RAM. En Linux/WSL intenta habilitar audio vía PulseAudio o SDL; en macOS vía CoreAudio.
Si ninguna opción de audio funciona, arranca sin audio.

---

## Comandos y tests

Todos los comandos se ejecutan como **procesos de usuario independientes**: la shell
nunca ejecuta comandos de forma inline; cada uno corre en un proceso hijo separado
creado con `create_process`.

### Administración de procesos

| Comando | Firma | Descripción |
|---------|-------|-------------|
| `ps` | `ps` | Lista todos los procesos vivos: PID, nombre, prioridad, estado (READY / RUNNING / BLOCKED), foreground (Y/N), RSP y RBP en hexadecimal. |
| `kill` | `kill <pid>` | Mata el proceso con el PID indicado. |
| `nice` | `nice <pid> <priority>` | Cambia la prioridad del proceso. `priority` ∈ [0, 4] (0 = máxima urgencia). |
| `block` | `block <pid>` | **Alterna el estado:** bloquea el proceso si está activo; lo desbloquea si ya está bloqueado. |
| `unblock` | `unblock <pid>` | Desbloquea el proceso indicado (sin toggle). |
| `loop` | `loop [priority]` | Crea un proceso en **background** que imprime su PID periódicamente (espera activa). Prioridad opcional en [0, 4], default 2. No termina solo; usar `kill` para detenerlo. |
| `yield` | `yield` | Cede la CPU voluntariamente (adelanta el cambio de contexto). |

### Semáforos

| Comando | Firma | Descripción |
|---------|-------|-------------|
| `sem open` | `sem open <nombre> <valor>` | Crea o abre un semáforo nombrado con el valor inicial dado. Imprime el `sem_id` asignado. |
| `sem close` | `sem close <sem_id>` | Cierra el semáforo (se destruye cuando nadie más lo referencia). |
| `sem wait` | `sem wait <sem_id>` | Operación P: decrementa el semáforo; bloquea al proceso si el valor es 0. |
| `sem post` | `sem post <sem_id>` | Operación V: incrementa el semáforo; despierta a un proceso bloqueado si los hay. |
| `sem value` | `sem value <sem_id>` | Muestra el valor actual del semáforo. |

### Productor-consumidor (MVar)

| Comando | Firma | Descripción |
|---------|-------|-------------|
| `mvar` | `mvar <n_writers> <n_readers>` | Lanza `n_writers` escritores y `n_readers` lectores en background que comparten una variable protegida por semáforos (patrón productor-consumidor clásico). Ambos valores ∈ [1, 10]. Los procesos corren indefinidamente; usar `kill` para detenerlos. |

### Pipes (stdin/stdout)

| Comando | Firma | Descripción |
|---------|-------|-------------|
| `cat` | `cat` | Copia stdin → stdout hasta EOF. |
| `wc` | `wc` | Cuenta las líneas recibidas por stdin e imprime el total. |
| `filter` | `filter` | Elimina vocales (mayúsculas y minúsculas) de stdin; pasa el resto a stdout. |

### Sistema y utilidades

| Comando | Firma | Descripción |
|---------|-------|-------------|
| `mem` | `mem` | Estado del heap: bytes totales, usados, libres y porcentaje de uso. |
| `echo` | `echo [texto...]` | Imprime los argumentos separados por espacios. |
| `clear` | `clear` | Limpia la pantalla. |
| `date` | `date` | Muestra la fecha actual. |
| `time` | `time` | Muestra la hora actual. |
| `sleep` | `sleep <ms>` | Pausa la ejecución `ms` milisegundos. |
| `shutdown` | `shutdown` | Apaga el sistema tras 5 segundos. |
| `registers` | `registers` | Imprime el snapshot de registros capturado con Shift+Tab. |

### Tests

| Test | Firma | Descripción |
|------|-------|-------------|
| `test_mm` | `test_mm <max_memory_bytes>` | Fuzz del administrador de memoria: aloca bloques aleatorios hasta `max_memory_bytes` bytes en total, escribe un patrón, verifica la integridad y libera. Corre hasta que se presiona cualquier tecla; imprime estadísticas finales. |
| `test_proc` | `test_proc <max_processes>` | Crea `max_processes` procesos dummy y los mata, bloquea y desbloquea aleatoriamente en loop continuo. No termina solo. |
| `test_sync` | `test_sync <n> <pairs> <use_sem>` | Crea `pairs` pares de procesos (un incrementador y un decrementador por par); cada proceso realiza `n` operaciones sobre una variable global compartida. Con `use_sem=1` usa un semáforo mutex (resultado final esperado: 0); con `use_sem=0` corre sin sincronización y exhibe race conditions. **Orden de parámetros: primero `n` (iteraciones por proceso), luego `pairs` (pares), luego `use_sem`.** |
| `test_prio` | `test_prio <max_value>` | Crea 3 procesos que cuentan hasta `max_value` en busy-loop. Observa el orden de finalización en tres fases: misma prioridad; distintas prioridades asignadas post-spawn; distintas prioridades asignadas con los procesos bloqueados. |

---

## Caracteres especiales

| Carácter | Efecto |
|----------|--------|
| `&` | Ejecuta el comando en **background**: la shell no espera a que termine y devuelve el prompt de inmediato, mostrando el PID del proceso creado. |
| `\|` | **Pipe entre dos comandos**: redirige stdout del primer comando al stdin del segundo. La shell espera a que ambos procesos terminen. |

**Restricciones:**

- Solo se admite un `|` por línea (no se soporta encadenamiento de más de 2 procesos).
- `|` y `&` no son combinables: en `cmd1 | cmd2 &`, el `&` se ignora silenciosamente.

---

## Atajos de teclado

| Atajo | Efecto |
|-------|--------|
| **Ctrl+C** | Mata el proceso en foreground y devuelve el prompt a la shell. |
| **Ctrl+D** | Envía EOF al proceso en foreground (termina la lectura de stdin). |
| **Shift+Tab** | Captura un snapshot de registros de la CPU (recuperable con `registers`). |

---

## Ejemplos de uso

### Estado de la memoria

```
mem
```
Muestra el estado del heap del sistema: bytes totales, usados, libres y porcentaje de
uso.

### Background y administración de procesos

```
loop &
ps
```
`loop &` crea un proceso en background que imprime su PID periódicamente. `ps` muestra
el proceso con foreground = N.

### Ciclo completo de administración: prioridad, bloqueo y kill

```
loop &
ps
nice <pid> 3
block <pid>
kill <pid>
```
Crea un proceso loop, baja su prioridad (3), lo bloquea (el comando `block`
alterna el estado) y finalmente lo mata.

### Pipes

```
cat | wc
```
Escribir varias líneas desde teclado y finalizar con **Ctrl+D**; `wc` imprime el número
de líneas recibidas.

```
cat | filter
```
El texto escrito llega a `filter`, que descarta las vocales y reenvía el resto a stdout.

### Foreground y Ctrl+C

```
cat
```
`cat` queda leyendo stdin; el texto escrito se reenvía a stdout. Presionar **Ctrl+C**
mata el proceso y devuelve el prompt.

### Semáforos desde la shell

```
sem open mutex 1
sem wait <id>
sem post <id>
sem close <id>
```
Crea un semáforo binario (mutex), lo adquiere, lo libera y lo destruye.

### Productor-consumidor

```
mvar 2 3
ps
```
Lanza 2 escritores y 3 readers que comparten una variable mediante semáforos. `ps`
muestra los 5 procesos corriendo en background.

### Sincronización: con y sin semáforo

```
test_sync 1000 3 0
test_sync 1000 3 1
```
Primera invocación: 3 pares de procesos hacen 1000 operaciones sin sincronización; el
valor final probablemente no es 0 (race condition visible). Segunda invocación: igual
pero con semáforo mutex; el valor final es siempre 0.

### Compilar con cada memory manager

```bash
# Buddy System (default) — desde x64BareBones/
./compile.sh

# Bitmap Memory Manager
docker exec -it tp-so bash -c \
    "make clean -C /root/Kernel && make bitmap -C /root/Kernel && make -C /root/Image"
```
Los dos memory managers son intercambiables sin modificar el resto del sistema.

---

## Convención de prioridades

El scheduler implementa **Round Robin por nivel de prioridad** con quantum fijo (5 ticks
de timer). El rango válido de prioridades es **[0, 4]**, donde:

- **0** = mayor urgencia (se ejecuta primero).
- **4** = menor urgencia (usada por el proceso `idle`).
- La shell corre con prioridad **2** (media).

**No hay aging:** un proceso en prioridad 0 ó 1 puede inanir indefinidamente a procesos
en prioridades más bajas.

---

## Requerimientos faltantes o parcialmente implementados

Todos los requerimientos del enunciado están implementados:

- Dos administradores de memoria intercambiables: Buddy System y Bitmap.
- Scheduler Round Robin multinivel con prioridades y quantum fijo.
- Semáforos nombrados con atomicidad garantizada por hardware (`xchg`).
- Pipes bloqueantes y transparentes (stdin/stdout redirigidos por el kernel).
- Shell con soporte de foreground, background (`&`) y pipes (`|`).
- Los cuatro tests de la cátedra (`test_mm`, `test_proc`, `test_sync`, `test_prio`)
  como procesos de usuario, ejecutables en foreground y background.

Las siguientes características **no fueron implementadas**, por decisión de diseño:

- Encadenamiento de más de 2 procesos con pipes (`cmd1 | cmd2 | cmd3`).
- Combinación simultánea de `|` y `&` en una misma línea.
- Aging / mecanismo anti-inanición en el scheduler.


---

## Base del proyecto y uso de IA


Nuestro propio **TP de Arquitectura de Computadoras** del cuatrimestre anterior, del
que reutilizamos los drivers de video y teclado, el manejo de interrupciones, la IDT
y el handler de syscalls. Sobre esa base se sumó todo lo nuevo de este TP: memoria
física, scheduler con prioridades, procesos, semáforos y pipes.

Durante el desarrollo usamos asistentes de IA (principalmente Claude Code) como apoyo
para implementación, debugging y revisión de código. Este README también fue redactado
con ayuda de IA. La lógica del código y las decisiones de diseño fueron revisadas y
validadas por los integrantes del grupo.
