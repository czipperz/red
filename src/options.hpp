#pragma once

#include <cz/buffer_array.hpp>
#include <cz/str.hpp>
#include <cz/vector.hpp>

namespace red {
struct Context;

struct Options {
    cz::Vector<const char*> input_files;
    cz::Vector<cz::Str> include_paths;
    cz::Buffer_Array buffer_array;

    int parse(Context*, int argc, char** argv);
    void destroy();
};

}
