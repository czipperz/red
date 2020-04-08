#pragma once

#include <cz/allocator.hpp>
#include <cz/vector.hpp>
#include "str_map.hpp"

namespace red {
struct File;

struct Files {
    cz::Vector<File> files;
    red::StrMap<size_t> indexes;

    void destroy(cz::Allocator allocator);
};

}
