#pragma once

#include <stddef.h>
#include <cz/allocator.hpp>
#include <cz/write.hpp>
#include "result.hpp"

namespace red {

struct File_Contents {
    static constexpr const size_t buffer_size_bits = 13;
    static constexpr const size_t buffer_size = 1 << buffer_size_bits;

    static constexpr const size_t inner_mask = buffer_size - 1;
    static constexpr const size_t outer_mask = ((size_t)-1) - inner_mask;

    char** buffers;
    size_t buffers_len;
    size_t len;

    Result read(const char* cstr_file_name, cz::Allocator buffers_array_allocator);
    void load_str(cz::Str contents, cz::Allocator buffers_array_allocator);

    void drop_buffers();
    void drop_array(cz::Allocator buffers_array_allocator);

    size_t get_base(size_t index) const { return (index & outer_mask) >> buffer_size_bits; }

    size_t get_offset(size_t index) const { return index & inner_mask; }

    char get(size_t index) const {
        CZ_DEBUG_ASSERT(index < len);
        return buffers[get_base(index)][get_offset(index)];
    }
};

}

namespace cz {

Result write(Writer, red::File_Contents);

}
