// ---------------------------------------------------------------------
// shell.c
// Shell interactiva con soporte de:
//   - Pipes:       cmd1 | cmd2
//   - Background:  cmd &
//   - Ctrl+C:      cancela la línea actual (read retorna 0 con ^C)
//   - Ctrl+D:      EOF en la lectura (trata la línea como vacía)
// ---------------------------------------------------------------------
#include "./include/syscall_call.h"
#include "./include/commandRead.h"
#include "./utils/utils.h"
#include "./tron2/include/map.h"
#include "./tron2/include/types.h"

// ---------------------------------------------------------------------
// Contextos estáticos para los dos lados de un pipe.
// Se llenan justo antes de crear cada proceso; como el scheduler sólo
// cambia de contexto en IRQ0, los procesos no leen los contextos hasta
// al menos un tick después de ser creados.
// ---------------------------------------------------------------------
typedef struct {
    char cmd[256];
    int  pipe_id;
} PipeProcCtx;

static PipeProcCtx g_pipe_ctx_a;   // cmd1: stdout → pipe
static PipeProcCtx g_pipe_ctx_b;   // cmd2: stdin  ← pipe
static char        g_bg_cmd[256];  // comando de background

// ---------------------------------------------------------------------
// Helpers de string sin libc
// ---------------------------------------------------------------------
static int sh_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void sh_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void sh_trim_right(char *s) {
    int n = sh_strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t')) {
        s[--n] = '\0';
    }
}

static char *sh_trim_left(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

// ---------------------------------------------------------------------
// Espera a que un proceso desaparezca de la tabla de procesos.
// ---------------------------------------------------------------------
static void wait_for_pid(int pid) {
    ProcessInfo buf[64];
    while (1) {
        int n = ps(buf, 64);
        int found = 0;
        for (int i = 0; i < n; i++) {
            if ((int)buf[i].pid == pid) { found = 1; break; }
        }
        if (!found) return;
        yield();
    }
}

// ---------------------------------------------------------------------
// Entradas de los procesos de pipe.
// Cada una configura su propio fd antes de ejecutar el comando.
// ---------------------------------------------------------------------
static void pipe_proc_a(void) {
    set_fd(1, g_pipe_ctx_a.pipe_id);           // stdout → pipe
    cr_dispatch_exact(g_pipe_ctx_a.cmd);
    pipe_close(g_pipe_ctx_a.pipe_id, 1);        // cierra extremo escritura
    exit_process();
}

static void pipe_proc_b(void) {
    set_fd(0, g_pipe_ctx_b.pipe_id);            // stdin ← pipe
    cr_dispatch_exact(g_pipe_ctx_b.cmd);
    pipe_close(g_pipe_ctx_b.pipe_id, 0);         // cierra extremo lectura
    exit_process();
}

// ---------------------------------------------------------------------
// Entrada del proceso de background.
// ---------------------------------------------------------------------
static void bg_proc_entry(void) {
    cr_dispatch_exact(g_bg_cmd);
    exit_process();
}

// ---------------------------------------------------------------------
// Ejecutar cmd1 | cmd2.
// ---------------------------------------------------------------------
static void run_piped(char *buf, int pipe_pos) {
    // Separar en cmd1 y cmd2
    buf[pipe_pos] = '\0';
    char *cmd1 = buf;
    char *cmd2 = sh_trim_left(buf + pipe_pos + 1);
    sh_trim_right(cmd1);

    if (sh_strlen(cmd1) == 0 || sh_strlen(cmd2) == 0) {
        println("Error: pipe mal formado.");
        return;
    }

    int pid_pipe = pipe_create();
    if (pid_pipe < 0) {
        println("Error: no se pudo crear el pipe.");
        return;
    }

    // Contexto para cmd1 (escribe al pipe)
    sh_strcpy(g_pipe_ctx_a.cmd, cmd1, 256);
    g_pipe_ctx_a.pipe_id = pid_pipe;

    // Contexto para cmd2 (lee del pipe)
    sh_strcpy(g_pipe_ctx_b.cmd, cmd2, 256);
    g_pipe_ctx_b.pipe_id = pid_pipe;

    int pid1 = create_process((void *)pipe_proc_a, "pipe_l", 2, 1, 0, 0);
    int pid2 = create_process((void *)pipe_proc_b, "pipe_r", 2, 1, 0, 0);

    if (pid1 > 0) wait_for_pid(pid1);
    if (pid2 > 0) wait_for_pid(pid2);
}

// ---------------------------------------------------------------------
// Ejecutar cmd en background (no esperar).
// ---------------------------------------------------------------------
static void run_background(char *buf) {
    sh_strcpy(g_bg_cmd, buf, 256);
    int pid = create_process((void *)bg_proc_entry, "bg", 2, 0, 0, 0);
    if (pid < 0) {
        println("Error: no se pudo crear el proceso en background.");
    } else {
        char num[12];
        write("[");
        uintToBase((uint64_t)pid, num, 10);
        write(num);
        write("]\n");
    }
}

// ---------------------------------------------------------------------
// Dispatcher principal: detecta |, &, o comando simple.
// ---------------------------------------------------------------------
static void shell_dispatch(char *buf) {
    int len = sh_strlen(buf);
    if (len == 0) return;

    // ¿Termina en & ? → background
    int background = 0;
    if (buf[len - 1] == '&') {
        background = 1;
        buf[--len] = '\0';
        sh_trim_right(buf);
        len = sh_strlen(buf);
        if (len == 0) return;
    }

    // ¿Contiene | ? → pipe (no compatible con & en esta implementación)
    int pipe_pos = -1;
    for (int i = 0; i < len; i++) {
        if (buf[i] == '|') { pipe_pos = i; break; }
    }

    if (pipe_pos >= 0) {
        run_piped(buf, pipe_pos);
    } else if (background) {
        run_background(buf);
    } else {
        cr_dispatch_exact(buf);
    }
}

// ---------------------------------------------------------------------
// Punto de entrada del shell
// ---------------------------------------------------------------------
int main(void) {
    char buf[256];
    while (1) {
        write("- ");
        int n = read(buf);
        // n == 0 cuando el usuario presionó Ctrl+C (ya se imprimió ^C)
        // o Ctrl+D en línea vacía: simplemente volver al prompt.
        if (n == 0) continue;
        shell_dispatch(buf);
    }
    return 0;
}
