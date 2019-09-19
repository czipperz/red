#pragma once

#include <stddef.h>
#include "file_buffer.hpp"
#include "location.hpp"

namespace red {
namespace cpp {

char next_character(const FileBuffer& file_buffer, Location* location);

}
}
