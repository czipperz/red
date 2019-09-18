#include "compiler.hpp"

#include <cz/defer.hpp>
#include <cz/try.hpp>
#include "preprocess.hpp"

namespace red {

Result compile_file(C* c, const char* file_name) {
    FileBuffer file_buffer;
    // this must be the same as in preprocess when we include a file
    CZ_TRY(file_buffer.read(file_name, c->allocator));

    Preprocessor preprocessor;
    CZ_DEFER(preprocessor.destroy(c));
    preprocessor.push(c, file_name, file_buffer);

    Result result;
    cz::AllocatedString label_value;
    label_value.allocator = c->allocator;
    CZ_DEFER(label_value.drop());
    Token token;
    while ((result = preprocessor.next(c, &token, &label_value)).is_ok()) {
        if (result.type == Result::Done) {
            result.type = Result::Success;
            break;
        }
    }

    return result;
}

}
