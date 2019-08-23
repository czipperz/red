#include <cz/assert.hpp>
#include <cz/mem/heap.hpp>
#include <cz/slice.hpp>
#include "compiler.hpp"
#include "context.hpp"

namespace red {

static void log(void*, cz::log::LogInfo info) {
    FILE* out;
    if (info.level < cz::log::LogLevel::Warning) {
        out = stderr;
    } else {
        out = stdout;
    }

    cz::io::write(cz::io::file_writer(out), info.level, ": ", info.message, '\n');
}

static int run_main(C* c, char* program_name, int argc, char** argv) {
    for (size_t i = 0; i < argc; ++i) {
        compile_file(c, argv[i]);
    }
    return 0;
}

int main(int argc, char** argv) {
    char* program_name = argv[0];
    argc--;
    argv++;

    cz::mem::AlignedBuffer<4096> temp_buffer;
    cz::mem::TempArena temp;
    temp.arena.mem = temp_buffer;

    Context context;
    context.allocator = cz::mem::heap_allocator();
    context.temp = &temp;
    context.logger = {log, NULL};
    context.max_log_level = cz::log::LogLevel::Debug;

    try {
        return run_main(&context, program_name, argc, argv);
    } catch (cz::PanicReachedException& e) {
        e.log(&context);
        return 1;
    }
}

}
