#include "options.hpp"

#include <cz/heap.hpp>
#include <cz/path.hpp>
#include "context.hpp"

namespace red {

void Options::destroy() {
    input_files.drop(cz::heap_allocator());
    include_paths.drop(cz::heap_allocator());
    buffer_array.drop();
}

int Options::parse(Context* context, int argc, char** argv) {
    buffer_array.create();

    include_paths.reserve(cz::heap_allocator(), 4);
    include_paths.push("/usr/local/include");
    include_paths.push("/usr/lib/gcc/x86_64-pc-linux-gnu/9.3.0/include-fixed");
    include_paths.push("/usr/include");
    include_paths.push("/usr/lib/gcc/x86_64-pc-linux-gnu/9.3.0/include");

    for (size_t i = 0; i < argc; ++i) {
        char* arg = argv[i];
        if (arg[0] == '-' && arg[1] == 'I') {
            include_paths.reserve(cz::heap_allocator(), 1);
            cz::Str relpath = arg + 2;
            cz::String path = {};
            if (cz::path::make_absolute(relpath, buffer_array.allocator(), &path).is_err()) {
                context->report_error_unspanned("Could not access working directory");
                return 1;
            }
            path.realloc_null_terminate(buffer_array.allocator());
            include_paths.push(path);
        } else {
            input_files.reserve(cz::heap_allocator(), 1);
            input_files.push(arg);
        }
    }

    return 0;
}

}
