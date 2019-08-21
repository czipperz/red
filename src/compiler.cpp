#include "compiler.hpp"

#include <cz/defer.hpp>
#include <cz/try.hpp>
#include "preprocess.hpp"

namespace red {

Result compile_file(C* c, const char* file_name) {
    Preprocessor preprocessor;
    CZ_DEFER(preprocessor.destroy(c));
    CZ_TRY(preprocessor.create(c, file_name));

    FILE* file = fopen("test_copy.txt", "w");
    CZ_DEFER(fclose(file));

    FileIndex file_index;
    for (char ch; (ch = preprocessor.next(c, &file_index)) != '\0';) {
        putc(ch, file);
    }

    return Result::ok();
}

}
