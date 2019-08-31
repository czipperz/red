#pragma once

#include <cz/mem/allocator.hpp>
#include <cz/vector.hpp>
#include "file_buffer.hpp"

namespace red {

struct Files {
    cz::SmallVector<FileBuffer, 0> buffers;
    cz::SmallVector<const char*, 0> names;

    void destroy(cz::mem::Allocator buffers_allocator, cz::mem::Allocator names_allocator);
};

}