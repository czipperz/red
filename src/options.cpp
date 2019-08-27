#include "options.hpp"

namespace red {

void Options::destroy(cz::C* c) {
    input_files.drop(c->allocator);
    include_paths.drop(c->allocator);
}

int Options::parse(cz::C* c, int argc, char** argv) {
    include_paths.reserve(c->allocator, 2);
    include_paths.push("/usr/local/include");
    include_paths.push("/usr/include");

    for (size_t i = 0; i < argc; ++i) {
        char* arg = argv[i];
        if (arg[0] == '-' && arg[1] == 'I') {
            include_paths.reserve(c->allocator, 1);
            include_paths.push(arg + 2);
        } else {
            input_files.reserve(c->allocator, 1);
            input_files.push(arg);
        }
    }

    return 0;
}

}
