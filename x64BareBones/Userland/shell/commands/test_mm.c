#include "../include/test_util.h"
#include "../include/syscall_call.h"
#include "../utils/utils.h"

#define MAX_BLOCKS 128

typedef struct MM_rq {
    void     *address;
    uint32_t  size;
} mm_rq;

void test_mm_command(int argc, char *argv[]) {
    mm_rq   mm_rqs[MAX_BLOCKS];
    uint8_t rq;
    uint32_t total;
    int64_t  max_memory;
    uint64_t iterations = 0;

    if (argc != 2) {
        println("Usage: test_mm <max_memory_bytes>");
        return;
    }

    max_memory = satoi(argv[1]);
    if (max_memory <= 0) {
        println("Error: max_memory must be > 0");
        return;
    }

    println("test_mm started. Press any key to stop.");
    println("");

    while (1) {
        rq = 0;
        total = 0;
        iterations++;

        while (rq < MAX_BLOCKS && total < max_memory) {
            mm_rqs[rq].size = GetUniform(max_memory - total - 1) + 1;
            mm_rqs[rq].address = malloc(mm_rqs[rq].size);

            if (mm_rqs[rq].address) {
                total += mm_rqs[rq].size;
                rq++;
            }
        }

        uint32_t i;
        for (i = 0; i < rq; i++)
            if (mm_rqs[i].address)
                memset(mm_rqs[i].address, (int32_t)(i & 0xFF), mm_rqs[i].size);

        for (i = 0; i < rq; i++)
            if (mm_rqs[i].address)
                if (!memcheck(mm_rqs[i].address, (uint8_t)(i & 0xFF), mm_rqs[i].size)) {
                    println("test_mm ERROR: memory corruption detected");
                    return;
                }

        for (i = 0; i < rq; i++)
            if (mm_rqs[i].address)
                free(mm_rqs[i].address);

        {
            char c = 0;
            int tries = 0;
            while (c == 0 && tries < 20) {
                c = getchar();
                tries++;
            }
            if (c != 0) {
                while (getchar() != 0) {}
                char buf[32];
                println("");
                println("+----------------------------------------+");
                println("|         test_mm FINAL RESULTS          |");
                println("+----------------------------------------+");
                write("| Iterations:  ");
                uintToBase(iterations, buf, 10);
                write(buf);
                uint64_t total_mem, used_mem, free_mem;
                mem_state(&total_mem, &used_mem, &free_mem);
                println("");

                write("| Total heap:  ");
                uintToBase(total_mem, buf, 10);
                println(buf);

                write("| Used:        ");
                uintToBase(used_mem, buf, 10);
                write(buf);
                if (used_mem == 0)
                    write(" (OK - all freed)");
                else
                    write(" (WARNING - memory leak!)");
                println("");

                write("| Free:        ");
                uintToBase(free_mem, buf, 10);
                println(buf);

                write("| Status:      PASS");
                println("");
                println("+----------------------------------------+");
                return;
            }
        }

        if (iterations % 50 == 0) {
            char buf[32];
            write("test_mm: iteration ");
            uintToBase(iterations, buf, 10);
            write(buf);
            write(", blocks: ");
            uintToBase((uint64_t)rq, buf, 10);
            println(buf);
        }
    }
}
