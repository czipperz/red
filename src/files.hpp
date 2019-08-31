#pragma once

#include <cz/vector.hpp>
#include "file_buffer.hpp"

namespace red {

struct Files {
    cz::SmallVector<FileBuffer, 0> buffers;
    cz::SmallVector<const char*, 0> names;
};

}
