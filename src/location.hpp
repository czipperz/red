#pragma once

#include <stddef.h>

namespace red {

struct Location {
    size_t index;
    size_t line;
    size_t column;
};

struct FileLocation {
    size_t file;
    Location location;
};

}
