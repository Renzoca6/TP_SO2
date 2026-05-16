// ---------------------------------------------------------------------
// pipe_cmds.c
// Comandos que operan sobre stdin/stdout para usarse con pipes.
//   cat    — reenvía stdin a stdout hasta EOF
//   wc     — cuenta líneas leídas desde stdin
//   filter — reenvía stdin a stdout eliminando las vocales
//   mem    — imprime el estado del heap
// ---------------------------------------------------------------------
#include "../include/syscall_call.h"
#include "../utils/utils.h"

// ---------------------------------------------------------------------
// cat: copia stdin → stdout hasta EOF (read retorna 0).
// ---------------------------------------------------------------------
void cmd_cat(int argc, char **argv) {
    (void)argc;
    (void)argv;

    char buf[256];
    int n;

    while (1) {
        n = read(buf);
        if (n == 0) break;      // EOF o Ctrl+D
        write(buf);
        write("\n");
    }
}

// ---------------------------------------------------------------------
// wc: cuenta líneas recibidas por stdin e imprime el total.
// ---------------------------------------------------------------------
void cmd_wc(int argc, char **argv) {
    (void)argc;
    (void)argv;

    char buf[256];
    int  lines = 0;

    while (1) {
        int n = read(buf);
        if (n == 0) break;      // EOF
        lines++;
    }

    char num[16];
    uintToBase((uint64_t)lines, num, 10);
    write(num);
    write("\n");
}

// ---------------------------------------------------------------------
// filter: lee de stdin, descarta vocales, escribe a stdout.
// ---------------------------------------------------------------------
static int is_vowel(char c) {
    return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
            c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U');
}

void cmd_filter(int argc, char **argv) {
    (void)argc;
    (void)argv;

    char in[256];
    char out[256];

    while (1) {
        int n = read(in);
        if (n == 0) break;      // EOF

        int j = 0;
        for (int i = 0; i < n; i++) {
            if (!is_vowel(in[i])) {
                out[j++] = in[i];
            }
        }
        out[j] = '\0';

        if (j > 0) {
            write(out);
            write("\n");
        }
    }
}

// ---------------------------------------------------------------------
// mem: muestra estado del heap (total / usado / libre / % uso).
// ---------------------------------------------------------------------
void cmd_mem(int argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t total = 0, used = 0, free_mem = 0;
    mem_state(&total, &used, &free_mem);

    char num[24];

    write("Total:  ");
    uintToBase(total, num, 10);
    write(num);
    write(" bytes\n");

    write("Used:   ");
    uintToBase(used, num, 10);
    write(num);
    write(" bytes\n");

    write("Free:   ");
    uintToBase(free_mem, num, 10);
    write(num);
    write(" bytes\n");

    // Porcentaje de uso (entero, sin punto flotante)
    uint64_t pct = (total > 0) ? (used * 100 / total) : 0;
    write("Usage:  ");
    uintToBase(pct, num, 10);
    write(num);
    write("%\n");
}
