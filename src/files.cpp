#include "files.hpp"

namespace red {

void Files::destroy(cz::Allocator allocator) {
    for (size_t i = 0; i < files.cap; ++i) {
        if (files.is_present(i)) {
            files.values[i].drop(allocator);
        }
    }
    files.drop(allocator);

    file_name_buffer_array.drop();

    indexes.drop(allocator);
}

}
