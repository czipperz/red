#include "file_buffer.hpp"

#include <cz/assert.hpp>
#include <cz/defer.hpp>

namespace red {

Result FileBuffer::read(const char* cstr_file_name,
                        cz::mem::Allocator buffer_allocator,
                        cz::mem::Allocator buffers_allocator) {
    FILE* file = fopen(cstr_file_name, "r");
    if (!file) {
        return Result::last_system_error();
    }
    CZ_DEFER(fclose(file));

    const size_t buffer_alignment = 1;

    size_t buffers_capacity = 8;
    buffers = static_cast<char**>(
        buffers_allocator.alloc({buffers_capacity * sizeof(char*), alignof(char*)}).buffer);
    buffers_len = 0;

    while (1) {
        // read a chunk into a new buffer
        char* buffer =
            static_cast<char*>(buffer_allocator.alloc({buffer_size, buffer_alignment}).buffer);
        size_t len = fread(buffer, 1, buffer_size, file);

        // empty chunk means eof so stop
        if (len == 0) {
            if (buffers_len == 0) {
                last_len = 0;
            } else {
                last_len = buffer_size;
            }
            buffer_allocator.dealloc({buffer, buffer_size});
            break;
        }

        // reserve a spot
        if (buffers_len + 1 == buffers_capacity) {
            size_t new_cap = buffers_capacity * 2;
            char** new_buffers =
                static_cast<char**>(buffers_allocator
                                        .realloc({buffers, buffers_capacity * sizeof(char*)},
                                                 {new_cap * sizeof(char*), alignof(char*)})
                                        .buffer);
            CZ_ASSERT(new_buffers);

            buffers = new_buffers;
            buffers_capacity = new_cap;
        }

        if (len == buffer_size) {
            // put the buffer in the spot and continue reading
            buffers[buffers_len] = buffer;
            ++buffers_len;
        } else {
            // shrink the last string to size
            last_len = len;
            char* shrunk_buffer = static_cast<char*>(
                buffer_allocator.realloc({buffer, buffer_size}, {last_len, buffer_alignment})
                    .buffer);
            CZ_ASSERT(shrunk_buffer);
            buffer = shrunk_buffer;

            // put it in the spot and stop reading
            buffers[buffers_len] = buffer;
            ++buffers_len;
            break;
        }
    }

    if (feof(file)) {
        // we most likely overallocated so shrink to size
        char** shrunk_buffers =
            static_cast<char**>(buffers_allocator
                                    .realloc({buffers, buffers_capacity * sizeof(char*)},
                                             {buffers_len * sizeof(char*), alignof(char*)})
                                    .buffer);
        CZ_ASSERT(shrunk_buffers);
        buffers = shrunk_buffers;

        return Result::ok();
    } else {
        // clean up
        for (size_t i = 0; i + 1 < buffers_len; ++i) {
            buffer_allocator.dealloc({buffers[i], buffer_size});
        }
        if (buffers_len > 0) {
            buffer_allocator.dealloc({buffers[buffers_len - 1], last_len});
        }
        buffers_allocator.dealloc({buffers, buffers_capacity * sizeof(char*)});

        buffers = 0;
        buffers_len = 0;
        last_len = 0;

        return {Result::ErrorFile};
    }
}

void FileBuffer::drop(cz::mem::Allocator buffer_allocator, cz::mem::Allocator buffers_allocator) {
    for (size_t i = 0; i + 1 < buffers_len; ++i) {
        buffer_allocator.dealloc({buffers[i], buffer_size});
    }
    if (buffers_len > 0) {
        buffer_allocator.dealloc({buffers[buffers_len - 1], last_len});
    }
    buffers_allocator.dealloc({buffers, buffers_len * sizeof(char*)});
}

}
