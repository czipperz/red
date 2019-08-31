#include <chrono>
#include <cz/assert.hpp>
#include <cz/logger.hpp>
#include <cz/mem/heap.hpp>
#include <cz/slice.hpp>
#include "compiler.hpp"
#include "context.hpp"

namespace red {

static FILE* choose_file(cz::log::LogLevel level) {
    if (level < cz::log::LogLevel::Warning) {
        return stderr;
    } else {
        return stdout;
    }
}

static cz::Result log_prefix(void*, const cz::log::LogInfo& info) {
    FILE* out = choose_file(info.level);
    return cz::write(cz::file_writer(out), info.level, ": ");
}

static cz::Result log_chunk(void*, const cz::log::LogInfo& info, cz::Str chunk) {
    FILE* out = choose_file(info.level);
    return cz::write(cz::file_writer(out), chunk);
}

static cz::Result log_suffix(void*, const cz::log::LogInfo& info) {
    FILE* out = choose_file(info.level);
    return cz::write(cz::file_writer(out), '\n');
}

static Result run_main(C* c) {
    for (size_t i = 0; i < c->options.input_files.len(); ++i) {
        CZ_TRY(compile_file(c, c->options.input_files[i]));
    }
    return Result::ok();
}

static int try_run_main(C* c) {
    try {
        Result result = run_main(c);

        for (size_t i = 0; i < c->errors.len(); ++i) {
            CZ_LOG(c, Error, c->errors[i].message);
        }

        if (result.is_err()) {
            CZ_LOG(c, Error, "Error code ", result.type);
            return 1;
        } else {
            return c->errors.len() > 0;
        }
    } catch (cz::PanicReachedException& e) {
        CZ_LOG(c, Fatal, e.what());
        return 2;
    }
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

    static const cz::log::Logger::VTable log_vtable = {log_prefix, log_chunk, log_suffix};
    context.logger = {&log_vtable, NULL};
    context.max_log_level = cz::log::LogLevel::Debug;

    context.program_name = program_name;

    CZ_DEFER(context.options.destroy(&context));
    if (context.options.parse(&context, argc, argv) != 0) {
        return 1;
    }

    CZ_DEFER({
        for (size_t i = 0; i < context.errors.len(); ++i) {
            context.errors[i].message.drop(context.allocator);
        }
        context.errors.drop(context.allocator);
    });

    // @TODO: Change second allocator here (name allocator) when we change file
    // names to use multi arena allocator.
    CZ_DEFER(context.files.destroy(context.allocator, context.allocator));

    auto start_time = std::chrono::high_resolution_clock::now();
    int code = try_run_main(&context);
    auto end_time = std::chrono::high_resolution_clock::now();

    size_t bytes = 0;
    for (size_t i = 0; i < context.files.buffers.len(); ++i) {
        bytes += context.files.buffers[i].len();
    }
    CZ_LOG(&context, Information, "Bytes processed: ", bytes);

    auto duration = end_time - start_time;
    using time_t = std::chrono::microseconds;
    auto as_micros = std::chrono::duration_cast<time_t>(duration).count();
    auto seconds = as_micros / time_t::period::den;
    auto micros = as_micros % time_t::period::den;

    CZ_LOG(&context, Information, "Elapsed: ", seconds, ".", micros, "s");

    return code;
}

}
