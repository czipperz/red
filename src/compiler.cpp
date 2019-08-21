#include "compiler.hpp"

#include <cz/defer.hpp>
#include <cz/try.hpp>
#include "buffer.hpp"

namespace red {

Result compile_file(C* c, const char* file_name) {
    cz::SmallVector<char*, 1> backlog;
    CZ_DEFER(backlog.drop(c->allocator));
    CZ_DEFER(for (size_t i = 0; i < backlog.len(); ++i) {
        c->allocator.dealloc({backlog[i], buffer_size});
    });

    cz::String last;
    CZ_DEFER(last.drop(c->allocator));

    CZ_TRY(read_file(file_name, c->allocator, c->allocator, &backlog, &last));

    Buffers file_buffers;
    file_buffers.backlog = backlog;
    file_buffers.last = last;

    return Result::ok();
}

}
