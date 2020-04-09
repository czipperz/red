#include "preprocess.hpp"

#include <ctype.h>
#include <cz/assert.hpp>
#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <cz/path.hpp>
#include <cz/try.hpp>
#include "context.hpp"
#include "definition.hpp"
#include "file.hpp"
#include "lex.hpp"
#include "load.hpp"
#include "result.hpp"
#include "token.hpp"

namespace red {
namespace cpp {

void Preprocessor::destroy() {
    file_pragma_once.drop(cz::heap_allocator());

    for (size_t i = 0; i < definitions.cap; ++i) {
        if (definitions.is_present(i)) {
            definitions.values[i].drop(cz::heap_allocator());
        }
    }
    definitions.drop(cz::heap_allocator());

    include_stack.drop(cz::heap_allocator());
    definition_stack.drop(cz::heap_allocator());
}

Location Preprocessor::location() const {
    return include_stack.last().location;
}

static void advance_over_whitespace(const File_Contents& contents, Location* location) {
    Location point = *location;
    while (1) {
        char ch;
        if (!lex::next_character(contents, &point, &ch)) {
            return;
        }
        if (!isspace(ch)) {
            return;
        }
        *location = point;
    }
}

static Result read_include(const File_Contents& contents,
                           Location* location,
                           Location* end_out,
                           char target,
                           cz::Allocator allocator,
                           cz::String* output) {
    while (1) {
        *end_out = *location;

        char ch;
        if (!lex::next_character(contents, location, &ch)) {
            return {Result::ErrorEndOfFile};
        }
        if (ch == target) {
            return Result::ok();
        }

        output->reserve(allocator, 1);
        output->push(ch);
    }
}

static Result process_include(Context* context,
                              Preprocessor* preprocessor,
                              lex::Lexer* lexer,
                              Token* token) {
    Location* point = &preprocessor->include_stack.last().location;
    advance_over_whitespace(context->files.files[point->file].contents, point);

    Location backup = *point;
    char ch;
    if (!lex::next_character(context->files.files[point->file].contents, point, &ch)) {
        return next_token(context, preprocessor, lexer, token);
    }

    if (ch != '<' && ch != '"') {
        *point = backup;
        CZ_PANIC("Unimplemented #include macro");
    }

    Span included_span;
    included_span.start = *point;
    cz::String relative_path = {};
    CZ_DEFER(relative_path.drop(context->temp_buffer_array.allocator()));
    CZ_TRY(read_include(context->files.files[point->file].contents, point, &included_span.end,
                        ch == '<' ? '>' : '"', context->temp_buffer_array.allocator(),
                        &relative_path));

    cz::String file_name = {};
    for (size_t i = context->options.include_paths.len() + 1; i-- > 0;) {
        cz::Str include_path;
        if (i == context->options.include_paths.len()) {
            // try local directory
            if (ch == '"') {
                cz::Option<cz::Str> dir =
                    cz::path::directory_component(context->files.files[point->file].path);
                // Todo: when this never triggers, remove it.
                CZ_ASSERT(dir.is_present);
                include_path = dir.value;
            } else {
                continue;
            }
        } else {
            // @Speed: store the include paths as Str s so we don't call strlen
            // over and over here
            include_path = context->options.include_paths[i];
        }

        bool trailing_slash = include_path.ends_with("/");  // @Speed: ends_with(char)
        file_name.set_len(0);
        file_name.reserve(context->files.file_buffer_array.allocator(),
                          include_path.len + !trailing_slash + relative_path.len() + 1);
        file_name.append(include_path);
        if (!trailing_slash) {
            file_name.push('/');
        }
        file_name.append(relative_path);
        cz::path::flatten(&file_name);
        file_name.null_terminate();

        if (include_file(&context->files, preprocessor, file_name).is_ok()) {
            return next_token(context, preprocessor, lexer, token);
        }
    }

    context->report_error(included_span, "Couldn't include file '", file_name, "'");
    file_name.drop(context->files.file_buffer_array.allocator());
    return {Result::ErrorInvalidInput};
}

static Result process_token(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token,
                            bool at_bol);

static Result process_next(Context* context,
                           Preprocessor* preprocessor,
                           lex::Lexer* lexer,
                           Token* token,
                           bool at_bol,
                           bool has_next);

static bool skip_until_eol(Context* context,
                           Preprocessor* preprocessor,
                           lex::Lexer* lexer,
                           Token* token) {
    Location* point = &preprocessor->include_stack.last().location;
    while (1) {
        bool at_bol = false;
        if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                             token, &at_bol)) {
            return false;
        }
        if (at_bol) {
            return true;
        }
    }
}

#define SKIP_UNTIL_EOL                                                            \
    ([&]() {                                                                      \
        bool at_bol = skip_until_eol(context, preprocessor, lexer, token);        \
        return process_next(context, preprocessor, lexer, token, at_bol, at_bol); \
    })

static Result process_pragma(Context* context,
                             Preprocessor* preprocessor,
                             lex::Lexer* lexer,
                             Token* token) {
    Location* point = &preprocessor->include_stack.last().location;
    bool at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point, token,
                         &at_bol)) {
        // #pragma is ignored and we are at eof.
        return next_token(context, preprocessor, lexer, token);
    }

    if (at_bol) {
        // #pragma is ignored and we are harboring a token.
        return process_token(context, preprocessor, lexer, token, at_bol);
    }

    if (token->type == Token::Identifier && token->v.identifier.str == "once") {
        preprocessor->file_pragma_once[point->file] = true;

        at_bol = false;
        if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                             token, &at_bol)) {
            // #pragma once \EOF
            return next_token(context, preprocessor, lexer, token);
        }

        if (!at_bol) {
            context->report_error(token->span, "#pragma once has trailing tokens");
            return SKIP_UNTIL_EOL();
        }

        // done processing the #pragma once so get the next token
        return process_token(context, preprocessor, lexer, token, at_bol);
    }

    context->report_error(token->span, "Unknown #pragma");
    return SKIP_UNTIL_EOL();
}

static Result process_if_true(Context* context,
                              Preprocessor* preprocessor,
                              lex::Lexer* lexer,
                              Token* token) {
    Location* point = &preprocessor->include_stack.last().location;
    bool at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point, token,
                         &at_bol)) {
        context->report_error({*point, *point}, "Unterminated preprocessing branch");
        return {Result::ErrorInvalidInput};
    }

    return process_token(context, preprocessor, lexer, token, at_bol);
}

static Result process_if_false(Context* context,
                               Preprocessor* preprocessor,
                               lex::Lexer* lexer,
                               Token* token) {
    size_t skip_depth = 0;
    while (1) {
        bool at_bol = skip_until_eol(context, preprocessor, lexer, token);
        if (!at_bol) {
            Location point = preprocessor->location();
            context->report_error({point, point}, "Unterminated #if");
            return {Result::ErrorInvalidInput};
        }

    check_hash:
        if (token->type == Token::Hash) {
            at_bol = false;
            Include_Info* info = &preprocessor->include_stack.last();
            if (!lex::next_token(context, lexer, context->files.files[info->location.file].contents,
                                 &info->location, token, &at_bol)) {
                Location point = preprocessor->location();
                context->report_error({point, point}, "Unterminated #if");
                return {Result::ErrorInvalidInput};
            }

            if (at_bol) {
                goto check_hash;
            }

            if (token->type == Token::Identifier) {
                if (token->v.identifier.str == "ifdef" && token->v.identifier.str == "ifndef" &&
                    token->v.identifier.str == "if") {
                    ++skip_depth;
                } else if (token->v.identifier.str == "else") {
                    if (skip_depth == 0) {
                        break;
                    }
                } else if (token->v.identifier.str == "endif") {
                    if (skip_depth > 0) {
                        --skip_depth;
                    } else {
                        CZ_DEBUG_ASSERT(info->if_depth > 0);
                        --info->if_depth;
                        break;
                    }
                }
            }
        }
    }

    return next_token(context, preprocessor, lexer, token);
}

static Result process_ifdef(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token,
                            bool want_present) {
    Span ifdef_span = token->span;

    Include_Info* point = &preprocessor->include_stack.last();
    bool at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->location.file].contents,
                         &point->location, token, &at_bol) ||
        at_bol) {
        context->report_error(ifdef_span, "No macro to test");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    if (token->type != Token::Identifier) {
        context->report_error(token->span, "Must test a macro name");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    point->if_depth++;

    if (!!preprocessor->definitions.get(token->v.identifier.str, token->v.identifier.hash) ==
        want_present) {
        return process_if_true(context, preprocessor, lexer, token);
    } else {
        return process_if_false(context, preprocessor, lexer, token);
    }
}

static Result process_if(Context* context,
                         Preprocessor* preprocessor,
                         lex::Lexer* lexer,
                         Token* token) {
    CZ_PANIC("Unimplemented #if");
}

static Result process_else(Context* context,
                           Preprocessor* preprocessor,
                           lex::Lexer* lexer,
                           Token* token) {
    // We just produced x and are skipping over y
    // #if 1
    // x
    // |#else
    // y
    // #endif

    Include_Info* point = &preprocessor->include_stack.last();
    if (point->if_depth == 0) {
        context->report_error(token->span, "#else without #if");
        return {Result::ErrorInvalidInput};
    }

    return process_if_false(context, preprocessor, lexer, token);
}

static Result process_endif(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token) {
    Include_Info* point = &preprocessor->include_stack.last();
    if (point->if_depth == 0) {
        context->report_error(token->span, "#endif without #if");
        return {Result::ErrorInvalidInput};
    }

    --point->if_depth;
    return SKIP_UNTIL_EOL();
}

static Result process_define(Context* context,
                             Preprocessor* preprocessor,
                             lex::Lexer* lexer,
                             Token* token) {
    Span define_span = token->span;

    Location* point = &preprocessor->include_stack.last().location;
    bool at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point, token,
                         &at_bol)) {
        context->report_error(define_span, "Must give the macro a name");
        return next_token(context, preprocessor, lexer, token);
    }
    if (at_bol) {
        context->report_error(define_span, "Must give the macro a name");
        return process_token(context, preprocessor, lexer, token, at_bol);
    }
    if (token->type != Token::Identifier) {
        context->report_error(token->span, "Must give the macro a name");
        return SKIP_UNTIL_EOL();
    }

    Hashed_Str identifier = token->v.identifier;

    Definition definition = {};
    definition.is_function = false;

    at_bol = false;
    while (1) {
        if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                             token, &at_bol)) {
            at_bol = false;
            break;
        }
        if (at_bol) {
            break;
        }

        definition.tokens.reserve(cz::heap_allocator(), 1);
        definition.tokens.push(*token);
    }

    preprocessor->definitions.reserve(cz::heap_allocator(), 1);
    preprocessor->definitions.insert(identifier.str, identifier.hash, definition);

    return process_next(context, preprocessor, lexer, token, at_bol, at_bol);
}

static Result process_undef(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token) {
    Span undef_span = token->span;

    Include_Info* point = &preprocessor->include_stack.last();
    bool at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->location.file].contents,
                         &point->location, token, &at_bol)) {
        context->report_error(undef_span, "Must specify the macro to undefine");
        return next_token(context, preprocessor, lexer, token);
    }
    if (at_bol) {
        context->report_error(undef_span, "Must specify the macro to undefine");
        return process_token(context, preprocessor, lexer, token, at_bol);
    }
    if (token->type != Token::Identifier) {
        context->report_error(token->span, "Must specify the macro to undefine");
        return SKIP_UNTIL_EOL();
    }

    preprocessor->definitions.remove(token->v.identifier.str, token->v.identifier.hash);

    return SKIP_UNTIL_EOL();
}

static Result process_error(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token) {
    context->report_error(token->span, "Explicit error");
    return SKIP_UNTIL_EOL();
}

static Result process_defined_identifier(Context* context,
                                         Preprocessor* preprocessor,
                                         lex::Lexer* lexer,
                                         Token* token,
                                         Definition* definition) {
    preprocessor->definition_stack.reserve(cz::heap_allocator(), 1);
    Definition_Info info = {};
    info.definition = definition;
    // Todo: process arguments
    preprocessor->definition_stack.push(info);
    return next_token(context, preprocessor, lexer, token);
}

static Result process_identifier(Context* context,
                                 Preprocessor* preprocessor,
                                 lex::Lexer* lexer,
                                 Token* token) {
    Definition* definition =
        preprocessor->definitions.get(token->v.identifier.str, token->v.identifier.hash);
    if (definition) {
        return process_defined_identifier(context, preprocessor, lexer, token, definition);
    } else {
        return Result::ok();
    }
}

static Result process_token(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token,
                            bool at_bol) {
top:
    Location* point = &preprocessor->include_stack.last().location;
    if (at_bol && token->type == Token::Hash) {
        at_bol = false;
        if (lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                            token, &at_bol)) {
            if (at_bol) {
                // #\n is ignored
                goto top;
            }

            if (token->type == Token::Identifier) {
                if (token->v.identifier.str == "include") {
                    return process_include(context, preprocessor, lexer, token);
                }
                if (token->v.identifier.str == "pragma") {
                    return process_pragma(context, preprocessor, lexer, token);
                }
                if (token->v.identifier.str == "ifdef") {
                    return process_ifdef(context, preprocessor, lexer, token, true);
                }
                if (token->v.identifier.str == "ifndef") {
                    return process_ifdef(context, preprocessor, lexer, token, false);
                }
                if (token->v.identifier.str == "endif") {
                    return process_endif(context, preprocessor, lexer, token);
                }
                if (token->v.identifier.str == "define") {
                    return process_define(context, preprocessor, lexer, token);
                }
                if (token->v.identifier.str == "undef") {
                    return process_undef(context, preprocessor, lexer, token);
                }
                if (token->v.identifier.str == "error") {
                    return process_error(context, preprocessor, lexer, token);
                }
            } else if (token->type == Token::If) {
                return process_if(context, preprocessor, lexer, token);
            } else if (token->type == Token::Else) {
                return process_else(context, preprocessor, lexer, token);
            }

            context->report_error(token->span, "Unknown preprocessor directive");
            return SKIP_UNTIL_EOL();
        }
    } else if (token->type == Token::Identifier) {
        return process_identifier(context, preprocessor, lexer, token);
    }

    return Result::ok();
}

static Result process_next(Context* context,
                           Preprocessor* preprocessor,
                           lex::Lexer* lexer,
                           Token* token,
                           bool at_bol,
                           bool has_next) {
    if (has_next) {
        return process_token(context, preprocessor, lexer, token, at_bol);
    } else {
        Location* point = &preprocessor->include_stack.last().location;
        if (point->index < context->files.files[point->file].contents.len) {
            Location end = *point;
            char ch;
            lex::next_character(context->files.files[point->file].contents, &end, &ch);
            context->report_error({*point, end}, "Invalid input '", ch, "'");
            return {Result::ErrorInvalidInput};
        } else {
            return next_token(context, preprocessor, lexer, token);
        }
    }
}

Result next_token(Context* context, Preprocessor* preprocessor, lex::Lexer* lexer, Token* token) {
    while (preprocessor->definition_stack.len() > 0) {
        Definition_Info* info = &preprocessor->definition_stack.last();
        if (info->index == info->definition->tokens.len()) {
            // This definition has ran through all its tokens.
            preprocessor->definition_stack.pop();
            continue;
        }

        Token* tk = &info->definition->tokens[info->index];
        if (tk->type == Token::Preprocessor_Parameter) {
            // Run through the tokens in the argument.
            cz::Slice<Token> argument_tokens = info->arguments[tk->v.integer];
            if (info->argument_index == argument_tokens.len) {
                // We're at the end of this argument.
                ++info->index;
                info->argument_index = 0;
                continue;
            }
            *token = argument_tokens[info->argument_index++];
        } else {
            ++info->index;
            // Since we postincrement index, we need to set `argument_index` to 0 if the next token
            // is a parameter token.  Since we don't use the value for any other reason and a branch
            // is expensive, we just always set it to 0.
            info->argument_index = 0;
            *token = *tk;
        }

        // Todo: handle # and ##

        if (token->type == Token::Identifier) {
            Definition* definition =
                preprocessor->definitions.get(token->v.identifier.str, token->v.identifier.hash);
            if (!definition) {
                return Result::ok();
            }

            // If we are already processing this definition, skip expanding it.
            for (size_t i = 0; i < preprocessor->definition_stack.len(); ++i) {
                if (preprocessor->definition_stack[i].definition == definition) {
                    return Result::ok();
                }
            }
            return process_defined_identifier(context, preprocessor, lexer, token, definition);
        }

        return Result::ok();
    }

    if (preprocessor->include_stack.len() == 0) {
        return Result::done();
    }

    Location* point = &preprocessor->include_stack.last().location;
    while (point->index == context->files.files[point->file].contents.len) {
        Include_Info entry = preprocessor->include_stack.pop();

        if (entry.if_depth > 0) {
            context->report_error({}, "Unterminated #if");
        }

        if (preprocessor->include_stack.len() == 0) {
            return Result::done();
        }

        point = &preprocessor->include_stack.last().location;
    }

    bool at_bol = point->index == 0;
    bool has_next = lex::next_token(context, lexer, context->files.files[point->file].contents,
                                    point, token, &at_bol);
    return process_next(context, preprocessor, lexer, token, at_bol, has_next);
}

}
}
