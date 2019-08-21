#include "buffer.hpp"

#include <cz/defer.hpp>

namespace red {

Result read_file(cz::mem::Allocator allocator,
                 const char* cstr_file_name,
                 cz::mem::Allocator backlog_allocator,
                 cz::Vector<char*>* backlog,
                 cz::Slice<char>* last) {
    FILE* file = fopen(cstr_file_name, "r");
    if (!file) {
        return Result::last_system_error();
    }
    CZ_DEFER(fclose(file));

    const size_t alignment = 1;

    char* buffer = static_cast<char*>(allocator.alloc({buffer_size, alignment}).buffer);
    size_t len = fread(buffer, 1, buffer_size, file);
    while (1) {
        if (len == buffer_size) {
            char* buffer2 = static_cast<char*>(allocator.alloc({buffer_size, alignment}).buffer);
            size_t len2 = fread(buffer2, 1, buffer_size, file);
            if (len2 != 0) {
                backlog->reserve(backlog_allocator, 1);
                backlog->push(buffer);
                buffer = buffer2;
                len = len2;
                continue;
            }
        }

        last->elems = buffer;
        last->len = len;

        if (feof(file)) {
            return Result::ok();
        } else {
            return {Result::ErrorFile};
        }
    }

    return Result::ok();
}

}
