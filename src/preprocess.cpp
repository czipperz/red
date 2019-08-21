#include "preprocess.hpp"

namespace red {

Result Preprocessor::create(C* c, const char* cstr_file_name) {
    file_buffers.reserve(c->allocator, 1);
    file_names.reserve(c->allocator, 1);

    FileBuffer buffer;
    CZ_TRY(buffer.read(cstr_file_name, c->allocator, c->allocator));

    file_buffers.push(buffer);
    file_names.push(cstr_file_name);
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

char Preprocessor::next(C* c, FileIndex* index) {
    return 0;
}

}
