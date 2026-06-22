#include "../include/syscall_call.h"
#include "../utils/utils.h"
#include <stddef.h>

static void append_str(char *dst, int *pos, const char *src, int max) {
    while (*src && *pos < max) {
        dst[(*pos)++] = *src++;
    }
}

static void pad_str(char *dst, int *pos, int width) {
    while (*pos < width) {
        dst[(*pos)++] = ' ';
    }
}

static const char *state_name(int state) {
    switch (state) {
        case 0:  return "READY   ";
        case 1:  return "RUNNING ";
        case 2:  return "BLOCKED ";
        case 3:  return "ZOMBIE  ";
        case 4:  return "KILLED  ";
        default: return "UNKNOWN ";
    }
}

void cmd_ps(int argc, char **argv) {
    (void)argc;
    (void)argv;

    ProcessInfo buf[64];
    int count = ps(buf, 64);

    if (count <= 0) {
        println("No processes found.");
        return;
    }

    println("PID        NAME              PRIO  STATE       FG  RSP          RBP");
    println("---------  ----------------  ----  ----------  --  -----------  -----------");

    for (int i = 0; i < count; i++) {
        char line[140];
        int pos = 0;

        char num[20];
        uintToBase(buf[i].pid, num, 10);
        append_str(line, &pos, num, 9);
        pad_str(line, &pos, 11);

        append_str(line, &pos, buf[i].name, 28);
        pad_str(line, &pos, 28);

        uintToBase((uint64_t)buf[i].priority, num, 10);
        append_str(line, &pos, num, 32);
        pad_str(line, &pos, 34);

        append_str(line, &pos, state_name(buf[i].state), 44);
        pad_str(line, &pos, 46);

        line[pos++] = buf[i].foreground ? 'Y' : 'N';
        line[pos++] = ' ';
        pad_str(line, &pos, 50);

        uintToBase(buf[i].rsp, num, 16);
        append_str(line, &pos, num, 61);
        pad_str(line, &pos, 63);

        uintToBase(buf[i].rbp, num, 16);
        append_str(line, &pos, num, 76);

        line[pos] = '\0';
        println(line);
    }
}

void cmd_kill(int argc, char **argv) {
    if (argc < 2 || !is_numeric(argv[1])) {
        println("Usage: kill <pid>");
        return;
    }
    int pid = string_to_int(argv[1]);
    kill_process((uint64_t)pid);
}

void cmd_nice(int argc, char **argv) {
    if (argc < 3 || !is_numeric(argv[1]) || !is_numeric(argv[2])) {
        println("Usage: nice <pid> <priority (0-4)>");
        return;
    }
    int pid = string_to_int(argv[1]);
    int prio = string_to_int(argv[2]);
    int result = nice((uint64_t)pid, prio);
    if (result < 0) {
        println("Error: invalid pid or priority.");
    }
}

void cmd_block(int argc, char **argv) {
    if (argc < 2 || !is_numeric(argv[1])) {
        println("Usage: block <pid>");
        return;
    }
    int pid = string_to_int(argv[1]);

    ProcessInfo buf[64];
    int count = ps(buf, 64);
    int is_blocked = 0;
    for (int i = 0; i < count; i++) {
        if ((int)buf[i].pid == pid) {
            is_blocked = (buf[i].state == 2);
            break;
        }
    }

    if (is_blocked) {
        unblock((uint64_t)pid);
    } else {
        block((uint64_t)pid);
    }
}

void cmd_unblock(int argc, char **argv) {
    if (argc < 2 || !is_numeric(argv[1])) {
        println("Usage: unblock <pid>");
        return;
    }
    int pid = string_to_int(argv[1]);
    unblock((uint64_t)pid);
}

static void loop_process(void) {
    while (1) {
        char pid_buf[8];
        uintToBase((uint64_t)getpid(), pid_buf, 10);
        write("Loop PID: ");
        write(pid_buf);
        write("\n");
        for (volatile uint64_t i = 0; i < 300000000; i++);
    }
}

void cmd_loop(int argc, char **argv) {
    int prio = 2;
    if (argc >= 2 && is_numeric(argv[1])) {
        prio = string_to_int(argv[1]);
        if (prio < 0) prio = 0;
        if (prio > 4) prio = 4;
    }

    int pid = create_process((void *)loop_process, "loop", prio, 0, 0, NULL);
    if (pid < 0) {
        println("Error: could not create loop process.");
    } else {
        char buf[8];
        uintToBase((uint64_t)pid, buf, 10);
        write("Loop process created with PID ");
        write(buf);
        write("\n");
    }
}

void cmd_yield(int argc, char **argv) {
    (void)argc;
    (void)argv;
    yield();
}