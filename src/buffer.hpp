#pragma once

#include <stdint.h>
#include <stdio.h>
#include <cz/assert.hpp>
#include <cz/string.hpp>
#include <cz/vector.hpp>
#include "result.hpp"

namespace red {

constexpr const size_t buffer_size_bits = 13;
constexpr const size_t buffer_size = 1 << buffer_size_bits;

struct Buffers {
    static constexpr const size_t inner_mask = buffer_size - 1;
    static constexpr const size_t outer_mask = ((size_t)-1) - inner_mask;

    cz::Slice<char*> backlog;
    cz::Str last;

    char get(size_t index) {
        const size_t outer = (index & outer_mask) >> buffer_size_bits;
        const size_t inner = index & inner_mask;

        if (outer < backlog.len) {
            return backlog[outer][inner];
        } else if (outer == backlog.len && inner < last.len) {
            return last[inner];
        } else {
            return '\0';
        }
    }
};

Result read_file(cz::mem::Allocator,
                 const char* cstr_file_name,
                 cz::Vector<char*>* backlog,
                 cz::Slice<char>* last);

}
