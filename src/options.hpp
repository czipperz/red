#pragma once

#include <cz/vector.hpp>

namespace red {

struct Options {
    cz::SmallVector<const char*, 0> input_files;
    cz::SmallVector<const char*, 0> include_paths;

    int parse(cz::C*, int argc, char** argv);
    void destroy(cz::C*);
};

}
