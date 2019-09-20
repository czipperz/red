#pragma once

#include <cz/vector.hpp>

namespace red {

struct Options {
    cz::Vector<const char*> input_files;
    cz::Vector<cz::Str> include_paths;

    int parse(cz::C*, int argc, char** argv);
    void destroy(cz::C*);
};

}
