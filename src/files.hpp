#pragma once

#include <cz/allocator.hpp>
#include <cz/vector.hpp>
#include "file_buffer.hpp"

namespace red {

struct Files {
    cz::Vector<FileBuffer> buffers;
    cz::Vector<const char*> names;

    void destroy(cz::Allocator allocator);
};

}
