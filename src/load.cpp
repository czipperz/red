#include "load.hpp"

namespace red {

Result load_file(C* c, cpp::Preprocessor* p, cz::String file_path) {
    Hash file_path_hash = StrMap<size_t>::hash(file_path);
    size_t* file_value = c->files.indexes.find(file_path, file_path_hash).value();
    if (file_value) {
        if (!p->file_pragma_once[*file_value]) {
            p->include_stack.reserve(c->allocator, 1);
            cpp::IncludeInfo info = {};
            info.location.file = *file_value;
            p->include_stack.push(info);
        }
    } else {
        c->files.buffers.reserve(c->allocator, 1);
        c->files.names.reserve(c->allocator, 1);
        c->files.indexes.reserve(c->allocator, 1);
        p->file_pragma_once.reserve(c->allocator, 1);
        p->include_stack.reserve(c->allocator, 1);

        FileBuffer file_buffer;
        CZ_TRY(file_buffer.read(file_path.buffer(), c->allocator));

        cpp::IncludeInfo info = {};
        info.location.file = c->files.buffers.len();

        c->files.buffers.push(file_buffer);
        c->files.names.push(file_path.buffer());
        c->files.indexes.find(file_path, file_path_hash).or_insert(info.location.file);
        p->file_pragma_once.push(false);
        p->include_stack.push(info);
    }

    return Result::ok();
}

}
