#include "files.hpp"

#include <cz/heap.hpp>
#include "file.hpp"

namespace red {

void Files::destroy() {
    for (size_t i = 0; i < files.len(); ++i) {
        files[i].contents.drop_buffers();
    }
    files.drop(cz::heap_allocator());
    file_path_hashes.drop(cz::heap_allocator());
    file_array_buffer_array.drop();
    file_path_buffer_array.drop();
}

}
