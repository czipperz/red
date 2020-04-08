#include <chrono>
#include <cz/assert.hpp>
#include <cz/heap.hpp>
#include <cz/log.hpp>
#include <cz/slice.hpp>
#include "compiler.hpp"
#include "context.hpp"

namespace red {

static FILE* choose_file(cz::LogLevel level) {
    if (level < cz::LogLevel::Warning) {
        return stderr;
    } else {
        return stdout;
    }
}

static cz::Result log_prefix(void*, const cz::LogInfo& info) {
    FILE* out = choose_file(info.level);
    return cz::write(cz::file_writer(out), info.level, ": ");
}

static cz::Result log_chunk(void*, const cz::LogInfo& info, cz::Str chunk) {
    FILE* out = choose_file(info.level);
    return cz::write(cz::file_writer(out), chunk);
}

static cz::Result log_suffix(void*, const cz::LogInfo& info) {
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
            const CompilerError& error = c->errors[i];
            const FileBuffer& buffer = c->files.buffers[error.span.start.file];
            const char* file_name = c->files.names[error.span.start.file];
            cz::write(cz::cerr(), "Error: ", file_name, ":", error.span.start.line + 1, ":",
                      error.span.start.column + 1, ": ", c->errors[i].message, ":\n");

            cz::write(cz::cerr(), "~   ");

            size_t line = error.span.start.line;
            size_t line_start = error.span.start.index - error.span.start.column;
            for (size_t i = line_start;; ++i) {
                cz::write(cz::cerr(), buffer.get(i));

                if (buffer.get(i) == '\n') {
                    cz::write(cz::cerr(), "    ");

                    size_t j = 0;
                    if (line == error.span.start.line) {
                        for (; j < error.span.start.column; ++j) {
                            cz::write(cz::cerr(), ' ');
                        }
                    }

                    if (line == error.span.end.line) {
                        for (; j < error.span.end.column; ++j) {
                            cz::write(cz::cerr(), '^');
                        }
                    } else {
                        for (; buffer.get(j + line_start) != '\n'; ++j) {
                            cz::write(cz::cerr(), '^');
                        }
                    }

                    cz::write(cz::cerr(), '\n');
                    if (i < error.span.end.index) {
                        cz::write(cz::cerr(), "~   ");
                    }

                    ++line;
                    line_start = i + 1;

                    if (i >= error.span.end.index) {
                        break;
                    }
                }
            }

            cz::write(cz::cerr(), '\n');
        }

        if (result.is_err()) {
            return 1;
        } else {
            return c->errors.len() > 0;
        }
    } catch (cz::PanicReachedException& e) {
        CZ_LOG(c, Fatal, "Compiler crash: ", e.what());
        return 2;
    }
}

int main(int argc, char** argv) {
    char* program_name = argv[0];
    argc--;
    argv++;

    Context context = {};
    context.init();
    CZ_DEFER(context.destroy());

    if (context.options.parse(&context, argc, argv) != 0) {
        return 1;
    }

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

    CZ_LOG(&context, Information, "Elapsed: ", seconds, ".", cz::format::width(6, micros), "s");

    return code;
}

}
