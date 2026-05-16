// ---------------------------------------------------------------------
// sem_cmds.c
// Comando `sem` para probar semáforos desde la shell:
//   sem open  <nombre> <valor>
//   sem close <sem_id>
//   sem wait  <sem_id>
//   sem post  <sem_id>
//   sem value <sem_id>
// ---------------------------------------------------------------------
#include "../include/syscall_call.h"
#include "../utils/utils.h"

void cmd_sem(int argc, char **argv) {
    if (argc < 2) {
        println("Usage: sem <open|close|wait|post|value> [args]");
        println("  sem open  <nombre> <valor>");
        println("  sem close <sem_id>");
        println("  sem wait  <sem_id>");
        println("  sem post  <sem_id>");
        println("  sem value <sem_id>");
        return;
    }

    char num[12];

    // sem open <nombre> <valor>
    if (argv[1][0] == 'o') {
        if (argc < 4 || !is_numeric(argv[3])) {
            println("Usage: sem open <nombre> <valor>");
            return;
        }
        int id = sem_open(argv[2], string_to_int(argv[3]));
        if (id < 0) {
            println("Error: sem_open failed.");
        } else {
            write("sem_id = ");
            uintToBase((uint64_t)id, num, 10);
            write(num);
            write("\n");
        }
        return;
    }

    // sem close <sem_id>
    if (argv[1][0] == 'c') {
        if (argc < 3 || !is_numeric(argv[2])) {
            println("Usage: sem close <sem_id>");
            return;
        }
        int ret = sem_close(string_to_int(argv[2]));
        println(ret == 0 ? "OK" : "Error: sem_close failed.");
        return;
    }

    // sem wait <sem_id>
    if (argv[1][0] == 'w') {
        if (argc < 3 || !is_numeric(argv[2])) {
            println("Usage: sem wait <sem_id>");
            return;
        }
        int ret = sem_wait(string_to_int(argv[2]));
        println(ret == 0 ? "OK (acquired)" : "Error: sem_wait failed.");
        return;
    }

    // sem post <sem_id>
    if (argv[1][0] == 'p') {
        if (argc < 3 || !is_numeric(argv[2])) {
            println("Usage: sem post <sem_id>");
            return;
        }
        int ret = sem_post(string_to_int(argv[2]));
        println(ret == 0 ? "OK (released)" : "Error: sem_post failed.");
        return;
    }

    // sem value <sem_id>
    if (argv[1][0] == 'v') {
        if (argc < 3 || !is_numeric(argv[2])) {
            println("Usage: sem value <sem_id>");
            return;
        }
        int val = sem_value(string_to_int(argv[2]));
        if (val < 0) {
            println("Error: sem_value failed.");
        } else {
            write("value = ");
            uintToBase((uint64_t)val, num, 10);
            write(num);
            write("\n");
        }
        return;
    }

    println("Unknown subcommand. Type 'sem' for usage.");
}
