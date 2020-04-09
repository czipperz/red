#include "file_contents.hpp"

#include <stdlib.h>
#include <cz/assert.hpp>
#include <cz/defer.hpp>

namespace red {

Result File_Contents::read(const char* cstr_file_name, cz::Allocator buffers_array_allocator) {
    FILE* file = fopen(cstr_file_name, "r");
    if (!file) {
        return Result::last_system_error();
    }
    CZ_DEFER(fclose(file));

    size_t buffers_capacity = 8;
    buffers = static_cast<char**>(
        buffers_array_allocator.alloc({buffers_capacity * sizeof(char*), alignof(char*)}));
    CZ_ASSERT(buffers);
    buffers_len = 0;

    while (1) {
        // read a chunk into a new buffer
        char* buffer = static_cast<char*>(malloc(buffer_size));
        CZ_ASSERT(buffer);
        size_t len = fread(buffer, 1, buffer_size, file);

        // empty chunk means eof so stop
        if (len == 0) {
            this->len = buffers_len * buffer_size;
            free(buffer);
            break;
        }

        // reserve a spot
        if (buffers_len + 1 == buffers_capacity) {
            size_t new_cap = buffers_capacity * 2;
            char** new_buffers = static_cast<char**>(
                buffers_array_allocator.realloc({buffers, buffers_capacity * sizeof(char*)},
                                                {new_cap * sizeof(char*), alignof(char*)}));
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
            this->len = buffers_len * buffer_size + len;
            char* shrunk_buffer = static_cast<char*>(realloc(buffer, len));
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
        char** shrunk_buffers = static_cast<char**>(
            buffers_array_allocator.realloc({buffers, buffers_capacity * sizeof(char*)},
                                            {buffers_len * sizeof(char*), alignof(char*)}));
        CZ_ASSERT(shrunk_buffers);
        buffers = shrunk_buffers;

        return Result::ok();
    } else {
        drop_array(buffers_array_allocator);
        return {Result::ErrorFile};
    }
}

void File_Contents::load_str(cz::Str contents, cz::Allocator allocator) {
    buffers_len = (contents.len + File_Contents::buffer_size - 1) / File_Contents::buffer_size;
    len = contents.len;
    buffers = static_cast<char**>(allocator.alloc({sizeof(char*) * buffers_len, alignof(char*)}));

    for (size_t i = 0; i < buffers_len; ++i) {
        size_t offset = i * File_Contents::buffer_size;
        size_t size;
        if (i + 1 < buffers_len) {
            size = File_Contents::buffer_size;
        } else {
            size = len - offset;
        }

        char* buffer = static_cast<char*>(malloc(size));
        buffers[i] = buffer;
        memcpy(buffer, contents.buffer + offset, size);
    }
}

void File_Contents::drop_buffers() {
    for (size_t i = 0; i < buffers_len; ++i) {
        free(buffers[i]);
    }
}

void File_Contents::drop_array(cz::Allocator allocator) {
    drop_buffers();
    allocator.dealloc({buffers, buffers_len * sizeof(char*)});
}

}

namespace cz {

Result write(Writer writer, red::File_Contents file_buffer) {
    for (size_t i = 0; i + 1 < file_buffer.buffers_len; ++i) {
        CZ_TRY(write(writer, Str{file_buffer.buffers[i], red::File_Contents::buffer_size}));
    }
    if (file_buffer.buffers_len > 0) {
        CZ_TRY(write(writer, Str{file_buffer.buffers[file_buffer.buffers_len - 1],
                                 file_buffer.len - (file_buffer.buffers_len - 1) *
                                                       red::File_Contents::buffer_size}));
    }
    return Result::ok();
}

}
