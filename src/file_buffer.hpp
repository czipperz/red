#pragma once

#include <stddef.h>
#include <cz/allocator.hpp>
#include <cz/write.hpp>
#include "result.hpp"

namespace red {

struct FileBuffer {
    static constexpr const size_t buffer_size_bits = 13;
    static constexpr const size_t buffer_size = 1 << buffer_size_bits;

    static constexpr const size_t inner_mask = buffer_size - 1;
    static constexpr const size_t outer_mask = ((size_t)-1) - inner_mask;

    char** buffers = 0;
    size_t buffers_len = 0;
    size_t last_len = 0;

    Result read(const char* cstr_file_name,
                cz::Allocator buffer_allocator,
                cz::Allocator buffers_allocator);

    void drop(cz::Allocator buffer_allocator, cz::Allocator buffers_allocator);

    char get(size_t index) const {
        const size_t outer = (index & outer_mask) >> buffer_size_bits;
        const size_t inner = index & inner_mask;

        if (outer + 1 < buffers_len || (outer + 1 == buffers_len && inner < last_len)) {
            return buffers[outer][inner];
        } else {
            return '\0';
        }
    }

    constexpr size_t len() const {
        return buffers_len == 0 ? 0 : ((buffers_len - 1) * buffer_size + last_len);
    }
};

}

namespace cz {

Result write(Writer, red::FileBuffer);

}
