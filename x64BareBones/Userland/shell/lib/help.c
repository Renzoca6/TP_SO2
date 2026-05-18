// ---------------------------------------------------------------------
// help.c
// Implementación del comando help, con caja de texto y búsqueda CI
// ---------------------------------------------------------------------
#include "../include/help.h"

// Traemos solo lo que necesitamos del entorno:
extern void println(const char *s);
extern int  write(const char *s);

// ---------------------------------------------------------------------
// Helpers 
// ---------------------------------------------------------------------
static int my_tolower_(int c) {
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static int ci_strcmp_(const char *a, const char *b) {
    for (;; a++, b++) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;

        int da = my_tolower_(ca);
        int db = my_tolower_(cb);

        if (da != db) return (da < db) ? -1 : 1;
        if (ca == 0)  return 0;   // llegaron ambos al '\0' => iguales
    }
}

static int my_strlen_(const char *s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

static void write_spaces_(int n) {
    char buf[64];
    while (n > 0) {
        int chunk = n > (int)(sizeof(buf) - 1) ? (int)(sizeof(buf) - 1) : n;
        for (int i = 0; i < chunk; i++) buf[i] = ' ';
        buf[chunk] = '\0';
        write(buf);
        n -= chunk;
    }
}

// ---------------------------------------------------------------------
// Dibujo de caja
// ---------------------------------------------------------------------
#define BOX_WIDTH 78    // Ancho de la caja (incluye los bordes)

// Línea horizontal: +-----...-----+
static void write_box_edge_(void) {
    char buf[BOX_WIDTH + 1];
    if (BOX_WIDTH < 2) return;

    buf[0] = '+';
    for (int i = 1; i < BOX_WIDTH - 1; i++)
        buf[i] = '-';
    buf[BOX_WIDTH - 1] = '+';
    buf[BOX_WIDTH] = '\0';

    write(buf);
    write("\n");
}

// Título centrado: |    AVAILABLE COMMANDS    |
static void write_box_title_(const char *title) {
    int inner = BOX_WIDTH - 2;
    int len   = my_strlen_(title);
    if (len > inner) len = inner;

    int left  = (inner - len) / 2;
    int right = inner - len - left;

    write("|");
    if (left > 0) write_spaces_(left);

    // escribir 'len' chars del título
    for (int i = 0; i < len; i++) {
        char b[2] = { title[i], 0 };
        write(b);
    }

    if (right > 0) write_spaces_(right);
    write("|");
    write("\n");
}

static void write_boxed_line_(const char *name, const char *text) {
    write("|");

    int inner = BOX_WIDTH - 2;
    int used  = 0;


    write(" ");
    used += 1;

    // name
    int ln = my_strlen_(name);
    write(name);
    used += ln;

    // separador " - "
    if (text && text[0]) {
        write(" - ");
        used += 3;
    }

    // texto (descripción) posible truncado si no entra
    if (text) {
        int lu   = my_strlen_(text);
        int room = inner - used;
        if (room < 0) room = 0;

        // escribir hasta 'room' chars
        for (int i = 0; i < lu && i < room; i++) {
            char buf[2] = { text[i], 0 };
            write(buf);
        }
        used += (lu < room) ? lu : room;
    }

    // rellenar con espacios si sobran
    if (used < inner) {
        write_spaces_(inner - used);
    }

    write("|");
    write("\n");
}

// ---------------------------------------------------------------------
// Diccionario de ayuda (nombre -> desc/uso)
// Debe estar ORDENADO case-insensitive igual que COMMANDS[]
// ---------------------------------------------------------------------
typedef struct {
    const char *name;
    const char *desc;
    const char *usage;
} help_entry_t;

static const help_entry_t HELP_ENTRIES[] = {
    { "benchmark",   "Run system benchmarks (FPS, FP ops, HW access).",      "benchmark" },
    { "block",       "Toggle block/unblock a process by PID.",           "block <pid>" },
    { "cat",         "Imprime stdin tal como lo recibe (util con pipes).",    "cat" },
    { "clear",       "Clear the screen.",                                    "clear" },
    { "date",        "Show the current date.",                               "date" },
    { "echo",        "Print the provided arguments.",                        "echo [args...]" },
    { "filter",      "Filtra las vocales del input (util con pipes).",       "filter" },
    { "fps",         "Run kernel FPS benchmark separately.",               "fps" },
    { "help",        "Show command help.",                                   "help [command]" },
    { "kill",        "Kill a process by PID.",                               "kill <pid>" },
    { "loop",        "Create a process that prints its PID periodically.",   "loop [priority]" },
    { "mem",         "Imprime el estado de la memoria del sistema.",          "mem" },
    { "mvar",        "Multiple readers/writers sobre MVar con semaforos.",  "mvar <n_writers> <n_readers>" },
    { "nice",        "Change process priority.",                              "nice <pid> <priority>" },
    { "ps",          "List all running processes.",                          "ps" },
    { "registers",   "Print the register snapshot captured with SHIFT+TAB.", "registers" },
    { "resize",      "Change font size (1-4).",                              "resize <1-4>" },
    { "sem",         "Gestion de semaforos nombrados.",                       "sem open/close/wait/post/value" },
    { "shutdown",    "Shutdown the system.",                                  "shutdown" },
    { "sleep",       "Pause execution for specified milliseconds.",          "sleep <ms>" },
    { "testinvalidop", "Trigger an invalid opcode exception (testing).",     "testinvalidop" },
    { "testmm",      "Memory manager fuzz test (alloc/write/check/free).",  "testmm <max_mem>" },
    { "testprio",    "Priority scheduler test with 3 processes.",         "testprio <max_value>" },
    { "testsound",   "Test system sound/beep functionality.",                "testsound" },
    { "testsyscalls","Run a complete test of all system calls.",             "testsyscalls" },
    { "testzero",    "Trigger a divide-by-zero exception (testing).",        "testzero" },
    { "time",        "Show the current time.",                               "time" },
    { "tron",        "Start the Tron game.",                                 "tron" },
    { "unblock",     "Unblock a process by PID.",                             "unblock <pid>" },
    { "wc",          "Cuenta la cantidad de lineas del input.",               "wc" },
    { "yield",       "Yield the CPU voluntarily.",                            "yield" },
};

// Busca por nombre en HELP_ENTRIES[]
static const help_entry_t *find_help_entry_(const char *name) {
    int lo = 0;
    int hi = (int)(sizeof(HELP_ENTRIES) / sizeof(HELP_ENTRIES[0]));

    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        int c   = ci_strcmp_(HELP_ENTRIES[mid].name, name);
        if (c == 0)
            return &HELP_ENTRIES[mid];
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return 0;
}

static int is_so2_command_(const char *name) {
    static const char *so2_list[] = {
        "ps", "kill", "nice", "block", "unblock", "loop",
        "cat", "wc", "filter", "mem", "sem", "mvar"
    };
    int n = (int)(sizeof(so2_list) / sizeof(so2_list[0]));
    for (int i = 0; i < n; i++) {
        const char *a = name;
        const char *b = so2_list[i];
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') return 1;
    }
    return 0;
}

static int is_test_command_(const char *name) {
    static const char *test_list[] = {
        "testmm", "testprio"
    };
    int n = (int)(sizeof(test_list) / sizeof(test_list[0]));
    for (int i = 0; i < n; i++) {
        const char *a = name;
        const char *b = test_list[i];
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') return 1;
    }
    return 0;
}

static void write_box_section_header_(const char *title) {
    int inner = BOX_WIDTH - 2;
    int len   = my_strlen_(title);
    if (len > inner) len = inner;

    int left  = (inner - len) / 2;
    int right = inner - len - left;

    write("|");
    if (left > 0) write_spaces_(left);
    for (int i = 0; i < len; i++) {
        char b[2] = { title[i], 0 };
        write(b);
    }
    if (right > 0) write_spaces_(right);
    write("|");
    write("\n");
}

static void help_list_all_(const command_t *comandos, int n) {
    write_box_edge_();
    write_box_title_("AVAILABLE COMMANDS");
    write_box_edge_();

    write_box_section_header_("--- Comandos TP-SO2 ---");

    for (int i = 0; i < n; i++) {
        if (!is_so2_command_(comandos[i].name))
            continue;
        const help_entry_t *h = find_help_entry_(comandos[i].name);
        write_boxed_line_(comandos[i].name, h ? h->desc : "");
    }

    write_box_section_header_("--- Comandos TP-Tests ---");

    for (int i = 0; i < n; i++) {
        if (!is_test_command_(comandos[i].name))
            continue;
        const help_entry_t *h = find_help_entry_(comandos[i].name);
        write_boxed_line_(comandos[i].name, h ? h->desc : "");
    }

    write_box_section_header_("--- Comandos TP-Arqui ---");

    for (int i = 0; i < n; i++) {
        if (is_so2_command_(comandos[i].name) || is_test_command_(comandos[i].name))
            continue;
        const help_entry_t *h = find_help_entry_(comandos[i].name);
        write_boxed_line_(comandos[i].name, h ? h->desc : "");
    }

    write_box_edge_();
}

static void help_one_(const char *name) {
    const help_entry_t *h = find_help_entry_(name);
    if (!h) {
        println("help: unknown command");
        return;
    }
    println("Command:");
    println(h->name);
    println("Description:");
    println(h->desc);
    println("Usage:");
    println(h->usage);
}

// ---------------------------------------------------------------------
// API del módulo
// ---------------------------------------------------------------------
int help_impl(const command_t *comandos, int n, int argc, char *argv[]) {
    // help <comando>
    if (argc >= 2 && argv[1] && argv[1][0]) {
        help_one_(argv[1]);
        return 0;
    }

    // help
    help_list_all_(comandos, n);
    return 0;
}
