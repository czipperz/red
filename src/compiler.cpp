#include "compiler.hpp"

#include <Tracy.hpp>
#include <cz/defer.hpp>
#include <cz/path.hpp>
#include <cz/try.hpp>
#include "context.hpp"
#include "hashed_str.hpp"
#include "lex.hpp"
#include "load.hpp"
#include "parse.hpp"
#include "preprocess.hpp"
#include "result.hpp"
#include "token.hpp"

namespace red {

Result compile_file(Context* context, const char* file_name) {
    ZoneScoped;

    parse::Parser parser = {};
    parser.init();
    CZ_DEFER(parser.drop());

    cz::String file_path = {};
    cz::Result abs_result = cz::path::make_absolute(
        file_name, context->files.file_path_buffer_array.allocator(), &file_path);
    if (abs_result.is_err()) {
        file_path.drop(context->files.file_path_buffer_array.allocator());
        return Result::from(abs_result);
    }
    file_path.realloc_null_terminate(context->files.file_path_buffer_array.allocator());

    CZ_TRY(include_file(&context->files, &parser.preprocessor, file_path));

    cz::Vector<parse::Statement*> initializers = {};
    while (1) {
        Result result = parse_declaration(context, &parser, &initializers);
        if (result.is_err()) {
            return result;
        }
        if (result.type == Result::Done) {
            return Result::ok();
        }
    }
}

}
