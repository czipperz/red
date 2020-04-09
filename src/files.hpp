#pragma once

#include <cz/allocator.hpp>
#include <cz/buffer_array.hpp>
#include <cz/str_map.hpp>
#include <cz/vector.hpp>

namespace red {
struct File;

struct Files {
    cz::Buffer_Array file_name_buffer_array;
    cz::Vector<File> files;

    void destroy(cz::Allocator allocator);
};

}
