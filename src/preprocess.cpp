#include "preprocess.hpp"

namespace red {

Result Preprocessor::create(C* c, const char* cstr_file_name) {
    file_buffers.reserve(c->allocator, 1);
    file_names.reserve(c->allocator, 1);
    include_stack.reserve(c->allocator, 1);

    FileBuffer buffer;
    CZ_TRY(buffer.read(cstr_file_name, c->allocator, c->allocator));

    file_buffers.push(buffer);
    file_names.push(cstr_file_name);
    include_stack.push({0, 0});

    return Result::ok();
}

void Preprocessor::destroy(C* c) {
    for (size_t i = 0; i < file_buffers.len(); ++i) {
        file_buffers[i].drop(c->allocator, c->allocator);
    }
    file_buffers.drop(c->allocator);
    file_names.drop(c->allocator);
    include_stack.drop(c->allocator);
}

char Preprocessor::next(C* c, FileIndex* index_out) {
    if (include_stack.len() == 0) {
        *index_out = {0, file_buffers[0].len()};
        return '\0';
    }

    FileIndex* point = &include_stack.last();
    while (point->index == file_buffers[point->file].len()) {
        if (include_stack.len() <= 1) {
            *index_out = {0, file_buffers[0].len()};
            return '\0';
        }

        include_stack.pop();
        point = &include_stack.last();
    }

    return file_buffers[point->file].get(point->index++);
}

}
