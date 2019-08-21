#include "compiler.hpp"

#include <cz/defer.hpp>
#include <cz/try.hpp>
#include "buffer.hpp"

namespace red {

Result compile_file(C* c, const char* file_name) {
    Buffer buffer;
    CZ_DEFER(buffer.drop(c->allocator, c->allocator));
    CZ_TRY(buffer.read(file_name, c->allocator, c->allocator));

    FILE* file = fopen("test_copy.txt", "w");
    CZ_DEFER(fclose(file));
    for (size_t i = 0; i < buffer.len(); ++i) {
        putc(buffer.get(i), file);
    }

    return Result::ok();
}

}
