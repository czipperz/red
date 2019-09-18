#pragma once

#include <cz/allocator.hpp>
#include <cz/vector.hpp>
#include "file_buffer.hpp"
#include "str_map.hpp"

namespace red {

struct Files {
    cz::Vector<FileBuffer> buffers;
    cz::Vector<const char*> names;
    red::StrMap<size_t> indexes;

    void destroy(cz::Allocator allocator);
};

}
