#pragma once

#include <cz/allocator.hpp>
#include <cz/str_map.hpp>
#include <cz/vector.hpp>

namespace red {
struct File;

struct Files {
    cz::Vector<File> files;
    cz::Str_Map<size_t> indexes;

    void destroy(cz::Allocator allocator);
};

}
