#include <chrono>
#include <stdint.h>
#include <inttypes.h>
#include <cz/assert.hpp>
#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <cz/slice.hpp>
#include <cz/try.hpp>
#include "compiler.hpp"
#include "context.hpp"
#include "file.hpp"
#include "result.hpp"

namespace red {

static Result run_main(Context* context) {
    for (size_t i = 0; i < context->options.input_files.len(); ++i) {
        CZ_TRY(compile_file(context, context->options.input_files[i]));
    }
    return Result::ok();
}

static int try_run_main(Context* context) {
    try {
        Result result = run_main(context);

        for (size_t i = 0; i < context->unspanned_errors.len(); ++i) {
            fprintf(stderr, "Error: ");
            cz::Str error = context->unspanned_errors[i];
            fwrite(error.buffer, 1, error.len, stderr);
            fputc('\n', stderr);
        }

        for (size_t i = 0; i < context->errors.len(); ++i) {
            const Compiler_Error& error = context->errors[i];
            const File& file = context->files.files[error.span.start.file];

            fputs("Error: ", stderr);
            fwrite(file.path.buffer, 1, file.path.len, stderr);
            fprintf(stderr, ":%zu:%zu: ", error.span.start.line + 1, error.span.start.column + 1);
            fwrite(context->errors[i].message.buffer, 1, context->errors[i].message.len, stderr);
            fputs(":\n~   ", stderr);

            size_t line = error.span.start.line;
            size_t line_start = error.span.start.index - error.span.start.column;
            for (size_t i = line_start;; ++i) {
                char ch = file.contents.get(i);
                putc(ch, stderr);

                if (ch == '\n') {
                    fputs("    ", stderr);

                    size_t j = 0;
                    if (line == error.span.start.line) {
                        for (; j < error.span.start.column; ++j) {
                            putc(' ', stderr);
                        }
                    }

                    if (line == error.span.end.line) {
                        for (; j < error.span.end.column; ++j) {
                            putc('^', stderr);
                        }
                    } else {
                        for (; file.contents.get(j + line_start) != '\n'; ++j) {
                            putc('^', stderr);
                        }
                    }

                    putc('\n', stderr);
                    if (i < error.span.end.index) {
                        fputs("~   ", stderr);
                    }

                    ++line;
                    line_start = i + 1;

                    if (i >= error.span.end.index) {
                        break;
                    }
                }
            }

            putc('\n', stderr);
        }

        if (result.is_err()) {
            return 1;
        } else {
            return context->unspanned_errors.len() > 0 || context->errors.len() > 0;
        }
    } catch (cz::PanicReachedException& e) {
        fprintf(stderr, "Fatal: Compiler crash: %s\n", e.what());
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
    for (size_t i = 0; i < context.files.files.len(); ++i) {
        bytes += context.files.files[i].contents.len;
    }
    printf("Bytes processed: %zu\n", bytes);

    auto duration = end_time - start_time;
    using std::chrono::microseconds;
    auto as_micros = std::chrono::duration_cast<microseconds>(duration).count();
    uint64_t seconds = as_micros / microseconds::period::den;
    uint64_t micros = as_micros % microseconds::period::den;

    printf("Elapsed: %" PRIu64 ".%.6" PRIu64 "s", seconds, micros);

    return code;
}

}
