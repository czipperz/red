#include <cz/assert.hpp>
#include <cz/logger.hpp>
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

static Result run_main(C* c) {
    for (size_t i = 0; i < c->options.input_files.len(); ++i) {
        CZ_TRY(compile_file(c, c->options.input_files[i]));
    }
    return Result::ok();
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
    context.program_name = program_name;
    CZ_DEFER(context.options.destroy(&context));
    if (context.options.parse(&context, argc, argv) != 0) {
        return 1;
    }

    try {
        Result result = run_main(&context);
        if (result.is_err()) {
            CZ_LOG(&context, Error, "Error code ", result.type);
            return 1;
        } else {
            return 0;
        }
    } catch (cz::PanicReachedException& e) {
        e.log(&context);
        return 2;
    }
}

}
