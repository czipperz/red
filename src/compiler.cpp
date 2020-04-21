#include "compiler.hpp"

#include <Tracy.hpp>
#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <cz/path.hpp>
#include <cz/stringify.hpp>
#include <cz/try.hpp>
#include "context.hpp"
#include "definition.hpp"
#include "file.hpp"
#include "file_contents.hpp"
#include "hashed_str.hpp"
#include "lex.hpp"
#include "load.hpp"
#include "parse.hpp"
#include "preprocess.hpp"
#include "result.hpp"
#include "token.hpp"

namespace red {

static void add_parse_definition(Context* context,
                                 parse::Parser* parser,
                                 Hashed_Str name,
                                 cz::Str contents) {
    cz::Vector<Token> tokens = {};

    Location point = {};
    point.file = context->files.files.len();

    cz::String file_path = {};
    file_path.reserve(parser->lexer.string_buffer_array.allocator(), 11 + name.str.len);
    file_path.append("*builtin ");
    file_path.append(name.str);
    file_path.append(" *");

    File_Contents file_contents;
    file_contents.load_str(contents, context->files.file_array_buffer_array.allocator());

    context->files.files.reserve(cz::heap_allocator(), 1);
    context->files.file_path_hashes.reserve(cz::heap_allocator(), 1);
    parser->preprocessor.file_pragma_once.reserve(cz::heap_allocator(), 1);

    File file;
    file.path = file_path;
    file.contents = file_contents;
    context->files.files.push(file);
    context->files.file_path_hashes.push(Hashed_Str::hash_str(file_path));
    parser->preprocessor.file_pragma_once.push(false);

    while (1) {
        Token token;
        bool at_bol = false;
        if (!lex::next_token(context, &parser->lexer, context->files.files[point.file].contents,
                             &point, &token, &at_bol)) {
            break;
        }

        tokens.reserve(cz::heap_allocator(), 1);
        tokens.push(token);
    }

    pre::Definition definition = {};
    definition.tokens = tokens;
    parser->preprocessor.definitions.insert(name.str, name.hash, definition);
}

#define ADD_BUILTIN_DEFINITION(x) \
    add_parse_definition(context, parser, Hashed_Str::from_str(#x), CZ_STRINGIFY(x));

static void add_builtin_definitions(Context* context, parse::Parser* parser) {
    ZoneScoped;

    parser->preprocessor.definitions.reserve(cz::heap_allocator(), 1024);

    ADD_BUILTIN_DEFINITION(__CHAR_UNSIGNED__);
    ADD_BUILTIN_DEFINITION(__WCHAR_UNSIGNED__);
    ADD_BUILTIN_DEFINITION(__REGISTER_PREFIX__);

    ADD_BUILTIN_DEFINITION(__SIZE_TYPE__);
    ADD_BUILTIN_DEFINITION(__PTRDIFF_TYPE__);
    ADD_BUILTIN_DEFINITION(__WCHAR_TYPE__);
    ADD_BUILTIN_DEFINITION(__WINT_TYPE__);
    ADD_BUILTIN_DEFINITION(__INTMAX_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINTMAX_TYPE__);
    ADD_BUILTIN_DEFINITION(__SIG_ATOMIC_TYPE__);
    /// These definitions cause int*_t to be typedef'd twice so I disabled them.
    // ADD_BUILTIN_DEFINITION(__INT8_TYPE__);
    // ADD_BUILTIN_DEFINITION(__INT16_TYPE__);
    // ADD_BUILTIN_DEFINITION(__INT32_TYPE__);
    // ADD_BUILTIN_DEFINITION(__INT64_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT8_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT16_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT32_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT64_TYPE__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST8_TYPE__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST16_TYPE__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST32_TYPE__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST64_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT_LEAST8_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT_LEAST16_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT_LEAST32_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT_LEAST64_TYPE__);
    ADD_BUILTIN_DEFINITION(__INT_FAST8_TYPE__);
    ADD_BUILTIN_DEFINITION(__INT_FAST16_TYPE__);
    ADD_BUILTIN_DEFINITION(__INT_FAST32_TYPE__);
    ADD_BUILTIN_DEFINITION(__INT_FAST64_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT_FAST8_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT_FAST16_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT_FAST32_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINT_FAST64_TYPE__);
    ADD_BUILTIN_DEFINITION(__INTPTR_TYPE__);
    ADD_BUILTIN_DEFINITION(__UINTPTR_TYPE__);

    ADD_BUILTIN_DEFINITION(__CHAR_BIT__);

    ADD_BUILTIN_DEFINITION(__SCHAR_MAX__);
    ADD_BUILTIN_DEFINITION(__WCHAR_MAX__);
    ADD_BUILTIN_DEFINITION(__SHRT_MAX__);
    ADD_BUILTIN_DEFINITION(__INT_MAX__);
    ADD_BUILTIN_DEFINITION(__LONG_MAX__);
    ADD_BUILTIN_DEFINITION(__LONG_LONG_MAX__);
    ADD_BUILTIN_DEFINITION(__WINT_MAX__);
    ADD_BUILTIN_DEFINITION(__SIZE_MAX__);
    ADD_BUILTIN_DEFINITION(__PTRDIFF_MAX__);
    ADD_BUILTIN_DEFINITION(__INTMAX_MAX__);
    ADD_BUILTIN_DEFINITION(__UINTMAX_MAX__);
    ADD_BUILTIN_DEFINITION(__SIG_ATOMIC_MAX__);
    ADD_BUILTIN_DEFINITION(__INT8_MAX__);
    ADD_BUILTIN_DEFINITION(__INT16_MAX__);
    ADD_BUILTIN_DEFINITION(__INT32_MAX__);
    ADD_BUILTIN_DEFINITION(__INT64_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT8_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT16_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT32_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT64_MAX__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST8_MAX__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST16_MAX__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST32_MAX__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST64_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT_LEAST8_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT_LEAST16_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT_LEAST32_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT_LEAST64_MAX__);
    ADD_BUILTIN_DEFINITION(__INT_FAST8_MAX__);
    ADD_BUILTIN_DEFINITION(__INT_FAST16_MAX__);
    ADD_BUILTIN_DEFINITION(__INT_FAST32_MAX__);
    ADD_BUILTIN_DEFINITION(__INT_FAST64_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT_FAST8_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT_FAST16_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT_FAST32_MAX__);
    ADD_BUILTIN_DEFINITION(__UINT_FAST64_MAX__);
    ADD_BUILTIN_DEFINITION(__INTPTR_MAX__);
    ADD_BUILTIN_DEFINITION(__UINTPTR_MAX__);
    ADD_BUILTIN_DEFINITION(__WCHAR_MIN__);
    ADD_BUILTIN_DEFINITION(__WINT_MIN__);
    ADD_BUILTIN_DEFINITION(__SIG_ATOMIC_MIN__);

    ADD_BUILTIN_DEFINITION(__INT8_C);
    ADD_BUILTIN_DEFINITION(__INT16_C);
    ADD_BUILTIN_DEFINITION(__INT32_C);
    ADD_BUILTIN_DEFINITION(__INT64_C);
    ADD_BUILTIN_DEFINITION(__UINT8_C);
    ADD_BUILTIN_DEFINITION(__UINT16_C);
    ADD_BUILTIN_DEFINITION(__UINT32_C);
    ADD_BUILTIN_DEFINITION(__UINT64_C);
    ADD_BUILTIN_DEFINITION(__INTMAX_C);
    ADD_BUILTIN_DEFINITION(__UINTMAX_C);

    ADD_BUILTIN_DEFINITION(__SCHAR_WIDTH__);
    ADD_BUILTIN_DEFINITION(__SHRT_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INT_WIDTH__);
    ADD_BUILTIN_DEFINITION(__LONG_WIDTH__);
    ADD_BUILTIN_DEFINITION(__LONG_LONG_WIDTH__);
    ADD_BUILTIN_DEFINITION(__PTRDIFF_WIDTH__);
    ADD_BUILTIN_DEFINITION(__SIG_ATOMIC_WIDTH__);
    ADD_BUILTIN_DEFINITION(__SIZE_WIDTH__);
    ADD_BUILTIN_DEFINITION(__WCHAR_WIDTH__);
    ADD_BUILTIN_DEFINITION(__WINT_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST8_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST16_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST32_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INT_LEAST64_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INT_FAST8_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INT_FAST16_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INT_FAST32_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INT_FAST64_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INTPTR_WIDTH__);
    ADD_BUILTIN_DEFINITION(__INTMAX_WIDTH__);

    ADD_BUILTIN_DEFINITION(__SIZEOF_INT__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_LONG__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_LONG_LONG__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_SHORT__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_POINTER__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_FLOAT__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_DOUBLE__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_LONG_DOUBLE__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_SIZE_T__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_WHAR_T__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_WINT_T__);
    ADD_BUILTIN_DEFINITION(__SIZEOF_PTRDIFF_T__);

    ADD_BUILTIN_DEFINITION(__BYTE_ORDER__);
    ADD_BUILTIN_DEFINITION(__ORDER_LITTLE_ENDIAN__);
    ADD_BUILTIN_DEFINITION(__ORDER_BIG_ENDIAN__);
    ADD_BUILTIN_DEFINITION(__ORDER_PDP_ENDIAN__);

    ADD_BUILTIN_DEFINITION(__FLOAT_WORD_ORDER__);

#ifdef _LP64
    ADD_BUILTIN_DEFINITION(__LP64__);
    ADD_BUILTIN_DEFINITION(_LP64);
#endif

    ADD_BUILTIN_DEFINITION(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1);
    ADD_BUILTIN_DEFINITION(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2);
    ADD_BUILTIN_DEFINITION(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4);
    ADD_BUILTIN_DEFINITION(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8);
    ADD_BUILTIN_DEFINITION(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16);
}

Result compile_file(Context* context, const char* file_name) {
    ZoneScoped;

    parse::Parser parser = {};
    parser.init();
    CZ_DEFER(parser.drop());

    add_builtin_definitions(context, &parser);

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
