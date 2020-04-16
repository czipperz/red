#include <inttypes.h>
#include <stdint.h>
#include <Tracy.hpp>
#include <chrono>
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
    ZoneScoped;
    for (size_t i = 0; i < context->options.input_files.len(); ++i) {
        CZ_TRY(compile_file(context, context->options.input_files[i]));
    }
    return Result::ok();
}

static void draw_error_span(const File_Contents* file_contents, Span error_span) {
    fputs("~   ", stderr);
    size_t line = error_span.start.line;
    size_t line_start = error_span.start.index - error_span.start.column;
    for (size_t i = line_start;; ++i) {
        char ch = file_contents->get(i);
        // Todo: handle tabs correctly
        if (ch == '\t') {
            putc(' ', stderr);
        } else {
            putc(ch, stderr);
        }

        if (ch == '\n') {
            fputs("    ", stderr);

            size_t j = 0;
            if (line == error_span.start.line) {
                for (; j < error_span.start.column; ++j) {
                    putc(' ', stderr);
                }
            }

            if (line == error_span.end.line) {
                for (; j < error_span.end.column; ++j) {
                    putc('^', stderr);
                }
            } else {
                for (; file_contents->get(j + line_start) != '\n'; ++j) {
                    putc('^', stderr);
                }
            }

            putc('\n', stderr);
            if (i < error_span.end.index) {
                fputs("~   ", stderr);
            }

            ++line;
            line_start = i + 1;

            if (i >= error_span.end.index) {
                break;
            }
        }
    }

    putc('\n', stderr);
}

static int try_run_main(Context* context) {
    ZoneScoped;
    try {
        Result result = run_main(context);

        ZoneScopedN("Show errors");
        for (size_t i = 0; i < context->unspanned_errors.len(); ++i) {
            fprintf(stderr, "Error: ");
            cz::Str error = context->unspanned_errors[i];
            fwrite(error.buffer, 1, error.len, stderr);
            fputc('\n', stderr);
        }

        for (size_t i = 0; i < context->errors.len(); ++i) {
            ZoneScoped;

            const Compiler_Error& error = context->errors[i];
            const File& source_file = context->files.files[error.source_span.start.file];

            fwrite(source_file.path.buffer, 1, source_file.path.len, stderr);
            fprintf(stderr, ":%zu:%zu: Error: ", error.source_span.start.line + 1,
                    error.source_span.start.column + 1);
            fwrite(context->errors[i].message.buffer, 1, context->errors[i].message.len, stderr);
            fputs(":\n", stderr);

            draw_error_span(&source_file.contents, error.source_span);

            if (error.error_span.start.file != error.source_span.start.file ||
                // error.error_span.start.index != error.source_span.start.index ||
                error.error_span.end.index != error.source_span.end.index) {
                const File& error_file = context->files.files[error.error_span.start.file];

                fwrite(error_file.path.buffer, 1, error_file.path.len, stderr);
                fprintf(stderr, ":%zu:%zu: Macro expanded from here:\n",
                        error.error_span.start.line + 1, error.error_span.start.column + 1);

                draw_error_span(&error_file.contents, error.error_span);
            }
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
    ZoneScoped;
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

    printf("Elapsed: %" PRIu64 ".%.6" PRIu64 "s\n", seconds, micros);

    return code;
}

}
