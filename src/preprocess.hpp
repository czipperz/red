#pragma once

#include <cz/vector.hpp>
#include "context.hpp"
#include "file_buffer.hpp"
#include "token.hpp"

namespace red {

struct FileIndex {
    size_t file;
    size_t index;
};

struct Preprocessor {
    cz::SmallVector<FileBuffer, 0> file_buffers;
    cz::SmallVector<const char*, 0> file_names;
    cz::SmallVector<FileIndex, 0> include_stack;

    Result create(C* c, const char* file_name, FileBuffer file_contents);
    void destroy(C* c);

    Result next(C* c,
                FileIndex* index_out,
                Token* token_out,
                cz::mem::Allocated<cz::String>* label_value);
};

}
