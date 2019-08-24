#include "preprocess.hpp"

#include <cz/assert.hpp>
#include <cz/logger.hpp>

namespace red {

Result Preprocessor::create(C* c, const char* file_name, FileBuffer file_buffer) {
    file_buffers.reserve(c->allocator, 1);
    file_names.reserve(c->allocator, 1);
    include_stack.reserve(c->allocator, 1);

    file_buffers.push(file_buffer);
    file_names.push(file_name);
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

Result Preprocessor::next(C* c,
                          FileIndex* index_out,
                          Token* token_out,
                          cz::mem::Allocated<cz::String>* label_value) {
    if (include_stack.len() == 0) {
        return Result::done();
    }

    FileIndex* point = &include_stack.last();
    while (point->index == file_buffers[point->file].len()) {
    pop_include:
        CZ_DEBUG_ASSERT(include_stack.len() >= 1);
        if (include_stack.len() == 1) {
            include_stack.pop();
            return Result::done();
        }

        include_stack.pop();
        point = &include_stack.last();
    }

    bool at_bol = false;
    Token token;
    if (next_token(file_buffers[point->file], &point->index, &token, &at_bol, label_value)) {
        *token_out = token;
        return Result::ok();
    } else {
        CZ_DEBUG_ASSERT(point->index <= file_buffers[point->file].len());
        if (point->index < file_buffers[point->file].len()) {
            CZ_LOG(c, Error, "Invalid input: ", file_buffers[point->file].get(point->index), " at ",
                   point->index);
            return {Result::ErrorInvalidInput};
        }
        goto pop_include;
    }
}

}
