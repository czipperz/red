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

char Preprocessor::next(C* c, FileIndex* index) {
    return 0;
}

}
