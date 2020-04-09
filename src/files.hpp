#pragma once

#include <cz/allocator.hpp>
#include <cz/buffer_array.hpp>
#include <cz/hash.hpp>
#include <cz/vector.hpp>

namespace red {
struct File;

struct Files {
    /// Used for file paths and the buffer array in `File_Contents`.
    cz::Buffer_Array file_path_buffer_array;
    cz::Buffer_Array file_array_buffer_array;
    cz::Vector<File> files;
    cz::Vector<cz::Hash> file_path_hashes;

    void init() {
        file_path_buffer_array.create();
        file_array_buffer_array.create();
    }
    void destroy();
};

}
