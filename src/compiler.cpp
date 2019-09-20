#include "compiler.hpp"

#include <cz/defer.hpp>
#include <cz/fs/working_directory.hpp>
#include <cz/try.hpp>
#include "load.hpp"
#include "preprocess.hpp"

namespace red {

Result compile_file(C* c, const char* file_name) {
    cpp::Preprocessor preprocessor;
    CZ_DEFER(preprocessor.destroy(c));

    cz::String file_path;
    cz::Result abs_result = cz::fs::make_absolute(file_name, c->allocator, &file_path);
    if (abs_result.is_err()) {
        file_path.drop(c->allocator);
        return Result::from(abs_result);
    }
    file_path.realloc_null_terminate(c->allocator);

    CZ_TRY(load_file(c, &preprocessor, file_path));

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
