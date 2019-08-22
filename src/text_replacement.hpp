#pragma once

#include <stddef.h>
#include "file_buffer.hpp"

namespace red {

char next_character(const FileBuffer& file_buffer, size_t* index);

}
