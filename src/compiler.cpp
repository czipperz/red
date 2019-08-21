#include "compiler.hpp"

#include <cz/defer.hpp>
#include <cz/try.hpp>
#include "file_buffer.hpp"

namespace red {

Result compile_file(C* c, const char* file_name) {
    FileBuffer file_buffer;
    CZ_TRY(file_buffer.read(file_name, c->allocator, c->allocator));
    CZ_DEFER(file_buffer.drop(c->allocator, c->allocator));

    FILE* file = fopen("test_copy.txt", "w");
    CZ_DEFER(fclose(file));
    for (size_t i = 0; i < file_buffer.len(); ++i) {
        putc(file_buffer.get(i), file);
    }

    return Result::ok();
}

}
