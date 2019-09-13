#pragma once

#include <stddef.h>

namespace red {

struct Location {
    size_t file;
    size_t index;
    size_t line;
    size_t column;
};

}
