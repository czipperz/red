#include "load.hpp"

namespace red {

void load_file_reserve(C* c, cpp::Preprocessor* p) {
    c->files.buffers.reserve(c->allocator, 1);
    c->files.names.reserve(c->allocator, 1);
    c->files.indexes.reserve(c->allocator, 1);
    p->file_pragma_once.reserve(c->allocator, 1);
    p->include_stack.reserve(c->allocator, 1);
}

void load_file_push(C* c,
                    cpp::Preprocessor* p,
                    cz::String file_path,
                    Hash file_path_hash,
                    FileBuffer file_buffer) {
    cpp::IncludeInfo info = {};
    info.location.file = c->files.buffers.len();

    c->files.buffers.push(file_buffer);
    c->files.names.push(file_path.buffer());
    c->files.indexes.find(file_path, file_path_hash).or_insert(info.location.file);
    p->file_pragma_once.push(false);
    p->include_stack.push(info);
}

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
        load_file_reserve(c, p);

        FileBuffer file_buffer;
        CZ_TRY(file_buffer.read(file_path.buffer(), c->allocator));

        load_file_push(c, p, file_path, file_path_hash, file_buffer);
    }

    return Result::ok();
}

}
