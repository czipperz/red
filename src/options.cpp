#include "options.hpp"

#include <cz/fs/working_directory.hpp>
#include <cz/log.hpp>

namespace red {

void Options::destroy(cz::C* c) {
    input_files.drop(c->allocator);

    for (size_t i = 4; i < include_paths.len(); ++i) {
        c->allocator.dealloc({const_cast<char*>(include_paths[i].buffer), include_paths[i].len});
    }
    include_paths.drop(c->allocator);
}

int Options::parse(cz::C* c, int argc, char** argv) {
    include_paths.reserve(c->allocator, 4);
    include_paths.push("/usr/local/include");
    include_paths.push("/usr/lib/gcc/x86_64-pc-linux-gnu/9.1.0/include-fixed");
    include_paths.push("/usr/include");
    include_paths.push("/usr/lib/gcc/x86_64-pc-linux-gnu/9.1.0/include");

    for (size_t i = 0; i < argc; ++i) {
        char* arg = argv[i];
        if (arg[0] == '-' && arg[1] == 'I') {
            include_paths.reserve(c->allocator, 1);
            cz::Str relpath = arg + 2;
            cz::String path;
            if (cz::fs::make_absolute(relpath, c->allocator, &path).is_err()) {
                CZ_LOG(c, Fatal, "Could not access working directory");
                return 1;
            }
            path.realloc_null_terminate(c->allocator);
            include_paths.push(path);
        } else {
            input_files.reserve(c->allocator, 1);
            input_files.push(arg);
        }
    }

    return 0;
}

}
