#include "files.hpp"

namespace red {

void Files::destroy(cz::Allocator buffers_allocator, cz::Allocator names_allocator) {
    for (size_t i = 0; i < buffers.len(); ++i) {
        buffers[i].drop(buffers_allocator, buffers_allocator);
    }
    buffers.drop(buffers_allocator);

    // @TODO: Remove when we change file names to use multi arena allocator.
    for (size_t i = 1; i < names.len(); ++i) {
        cz::Str file_name(names[i]);
        names_allocator.dealloc({const_cast<char*>(file_name.buffer), file_name.len});
    }
    names.drop(names_allocator);
}

}
