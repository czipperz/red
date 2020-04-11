#pragma once

#include <stddef.h>

namespace red {

struct Location {
    size_t file;
    size_t index;
    size_t line;
    size_t column;

    bool operator==(const Location& other) const {
        return file == other.file && index == other.index;
    }
    bool operator!=(const Location& other) const { return !(*this == other); }
};

}
