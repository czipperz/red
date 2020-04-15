#include "preprocess.hpp"

#include <ctype.h>
#include <Tracy.hpp>
#include <cz/assert.hpp>
#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <cz/path.hpp>
#include <cz/str_map.hpp>
#include <cz/try.hpp>
#include "context.hpp"
#include "definition.hpp"
#include "file.hpp"
#include "lex.hpp"
#include "load.hpp"
#include "result.hpp"
#include "token.hpp"

namespace red {
namespace pre {

void Preprocessor::destroy() {
    file_pragma_once.drop(cz::heap_allocator());

    for (size_t i = 0; i < definitions.cap; ++i) {
        if (definitions.is_present(i)) {
            definitions.values[i].drop(cz::heap_allocator());
        }
    }
    definitions.drop(cz::heap_allocator());

    for (size_t i = 0; i < include_stack.len(); ++i) {
        include_stack[i].if_stack.drop(cz::heap_allocator());
    }
    include_stack.drop(cz::heap_allocator());

    definition_stack.drop(cz::heap_allocator());
}

Location Preprocessor::location() const {
    return include_stack.last().location;
}

static Span source_span(Preprocessor* preprocessor) {
    Location end = preprocessor->location();
    return {end, end};
}

static void drop(Definition_Info* info) {
    for (size_t i = 0; i < info->arguments.len(); ++i) {
        info->arguments[i].drop(cz::heap_allocator());
    }
    info->arguments.drop(cz::heap_allocator());
}

static void advance_over_whitespace(const File_Contents& contents, Location* location) {
    ZoneScoped;
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
    ZoneScoped;
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
    ZoneScoped;
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
        file_name.reserve(context->files.file_path_buffer_array.allocator(),
                          include_path.len + !trailing_slash + relative_path.len() + 1);
        file_name.append(include_path);
        if (!trailing_slash) {
            file_name.push('/');
        }
        file_name.append(relative_path);
        cz::path::flatten(&file_name);
        file_name.null_terminate();

        if (include_file(&context->files, preprocessor, file_name).is_ok()) {
            relative_path.drop(context->temp_buffer_array.allocator());
            return next_token(context, preprocessor, lexer, token);
        }
    }

    context->report_lex_error(included_span, "Couldn't include file '", relative_path, "'");
    file_name.drop(context->files.file_path_buffer_array.allocator());
    relative_path.drop(context->temp_buffer_array.allocator());
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
    ZoneScoped;
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
    ZoneScoped;
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
            context->report_lex_error(token->span, "#pragma once has trailing tokens");
            return SKIP_UNTIL_EOL();
        }

        // done processing the #pragma once so get the next token
        return process_token(context, preprocessor, lexer, token, at_bol);
    }

    context->report_lex_error(token->span, "Unknown #pragma");
    return SKIP_UNTIL_EOL();
}

static Result process_if_true(Context* context,
                              Preprocessor* preprocessor,
                              lex::Lexer* lexer,
                              Token* token) {
    ZoneScoped;
    Location* point = &preprocessor->include_stack.last().location;
    bool at_bol = true;
    if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point, token,
                         &at_bol)) {
        context->report_lex_error({*point, *point}, "Unterminated preprocessing branch");
        return {Result::ErrorInvalidInput};
    }

    return process_token(context, preprocessor, lexer, token, at_bol);
}

static Result process_if(Context* context,
                         Preprocessor* preprocessor,
                         lex::Lexer* lexer,
                         Token* token);

static Result process_if_false(Context* context,
                               Preprocessor* preprocessor,
                               lex::Lexer* lexer,
                               Token* token,
                               bool start_at_bol,
                               bool allow_else) {
    ZoneScoped;
    size_t skip_depth = 0;

    bool at_bol;
    if (start_at_bol) {
        Location* point = &preprocessor->include_stack.last().location;
        at_bol = true;
        at_bol = lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                                 token, &at_bol);
        goto check_token_exists;
    }

    while (1) {
        at_bol = skip_until_eol(context, preprocessor, lexer, token);

    check_token_exists:
        if (!at_bol) {
            Include_Info entry = preprocessor->include_stack.last();
            for (size_t i = 0; i < entry.if_stack.len(); ++i) {
                context->report_lex_error(entry.if_stack[i], "Unterminated #if");
            }
            return {Result::ErrorInvalidInput};
        }

    check_hash:
        if (token->type == Token::Hash) {
            at_bol = false;
            Include_Info* info = &preprocessor->include_stack.last();
            if (!lex::next_token(context, lexer, context->files.files[info->location.file].contents,
                                 &info->location, token, &at_bol)) {
                Include_Info entry = preprocessor->include_stack.last();
                for (size_t i = 0; i < entry.if_stack.len(); ++i) {
                    context->report_lex_error(entry.if_stack[i], "Unterminated #if");
                }
                return {Result::ErrorInvalidInput};
            }

            if (at_bol) {
                goto check_hash;
            }

            if (token->type == Token::Identifier) {
                if (token->v.identifier.str == "ifdef" || token->v.identifier.str == "ifndef" ||
                    token->v.identifier.str == "if") {
                    ++skip_depth;
                } else if (token->v.identifier.str == "else") {
                    if (allow_else && skip_depth == 0) {
                        break;
                    }
                } else if (token->v.identifier.str == "elif") {
                    if (allow_else && skip_depth == 0) {
                        preprocessor->include_stack.last().if_stack.pop();
                        return process_if(context, preprocessor, lexer, token);
                    }
                } else if (token->v.identifier.str == "endif") {
                    if (skip_depth > 0) {
                        --skip_depth;
                    } else {
                        info->if_stack.pop();
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
    ZoneScoped;
    Span ifdef_span = token->span;

    Include_Info* point = &preprocessor->include_stack.last();
    bool at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->location.file].contents,
                         &point->location, token, &at_bol) ||
        at_bol) {
        context->report_lex_error(ifdef_span, "No macro to test");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    if (token->type != Token::Identifier) {
        context->report_lex_error(token->span, "Must test an identifier");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    point->if_stack.reserve(cz::heap_allocator(), 1);
    point->if_stack.push(ifdef_span);

    if (!!preprocessor->definitions.get(token->v.identifier.str, token->v.identifier.hash) ==
        want_present) {
        return process_if_true(context, preprocessor, lexer, token);
    } else {
        return process_if_false(context, preprocessor, lexer, token, false, true);
    }
}

static Result process_defined_identifier(Context* context,
                                         Preprocessor* preprocessor,
                                         lex::Lexer* lexer,
                                         Token* token,
                                         Definition* definition,
                                         bool this_line_only);

static Result next_token_in_definition(Context* context,
                                       Preprocessor* preprocessor,
                                       lex::Lexer* lexer,
                                       Token* token,
                                       bool this_line_only,
                                       int expand_macros);

static Result parse_and_eval_expression(Context* context,
                                        Span if_span,
                                        cz::Slice<Token> tokens,
                                        size_t* index,
                                        int64_t* value,
                                        int max_precedence) {
    ZoneScoped;

    if (*index == tokens.len) {
        CZ_DEBUG_ASSERT(*index >= 1);
        context->report_error(tokens[*index - 1].span, if_span, "Unterminated expression");
        return {Result::ErrorInvalidInput};
    }

    switch (tokens[*index].type) {
        case Token::Minus: {
            ++*index;
            CZ_TRY(parse_and_eval_expression(context, if_span, tokens, index, value, 0));
            *value = -*value;
            break;
        }

        case Token::Not: {
            ++*index;
            CZ_TRY(parse_and_eval_expression(context, if_span, tokens, index, value, 0));
            *value = !*value;
            break;
        }

        case Token::Integer: {
            *value = tokens[*index].v.integer.value;
            ++*index;
            break;
        }

        case Token::OpenParen: {
            Location open_paren = tokens[*index].span.start;
            ++*index;
            CZ_TRY(parse_and_eval_expression(context, if_span, tokens, index, value, 100));
            if (*index == tokens.len || tokens[*index].type != Token::CloseParen) {
                context->report_error({open_paren, tokens[*index - 1].span.end}, if_span,
                                      "Unterminated parenthesized expression");
                return {Result::ErrorInvalidInput};
            }
            ++*index;
            break;
        }

        default:
            context->report_error(tokens[*index].span, if_span, "Unexpected token `",
                                  tokens[*index], "` in #if expression");
            return {Result::ErrorInvalidInput};
    }

    while (1) {
        if (*index == tokens.len) {
            return Result::ok();
        }

        int precedence;
        Token::Type op = tokens[*index].type;
        switch (op) {
            case Token::CloseParen:
            case Token::Colon:
                return Result::ok();

            case Token::QuestionMark: {
                precedence = 16;
                bool ltr = false;

                if (precedence >= max_precedence) {
                    return Result::ok();
                }

                Span question_mark_span = tokens[*index].span;

                ++*index;
                int64_t then;
                CZ_TRY(parse_and_eval_expression(context, if_span, tokens, index, &then,
                                                 precedence + !ltr));

                if (*index == tokens.len || tokens[*index].type != Token::Colon) {
                    context->report_error(
                        question_mark_span, if_span,
                        "Expected `:` and then otherwise expression side for ternary operator");
                    return {Result::ErrorInvalidInput};
                }
                ++*index;

                int64_t otherwise;
                CZ_TRY(parse_and_eval_expression(context, if_span, tokens, index, &otherwise,
                                                 precedence + !ltr));

                *value = *value ? then : otherwise;
                continue;
            }

            case Token::LessThan:
            case Token::LessEqual:
            case Token::GreaterThan:
            case Token::GreaterEqual:
                precedence = 9;
                break;
            case Token::Equals:
            case Token::NotEquals:
                precedence = 10;
                break;
            case Token::Comma:
                precedence = 17;
                break;
            case Token::Plus:
            case Token::Minus:
                precedence = 6;
                break;
            case Token::Divide:
            case Token::Star:
                precedence = 5;
                break;
            case Token::Ampersand:
                precedence = 11;
                break;
            case Token::And:
                precedence = 14;
                break;
            case Token::Pipe:
                precedence = 13;
                break;
            case Token::Or:
                precedence = 15;
                break;
            case Token::Xor:
                precedence = 12;
                break;
            case Token::LeftShift:
                precedence = 7;
                break;
            case Token::RightShift:
                precedence = 7;
                break;
            default:
                context->report_error(tokens[*index].span, if_span,
                                      "Expected binary operator here to connect expressions");
                return {Result::ErrorInvalidInput};
        }

        if (precedence >= max_precedence) {
            return Result::ok();
        }

        ++*index;
        int64_t right;
        CZ_TRY(parse_and_eval_expression(context, if_span, tokens, index, &right, precedence));

        switch (op) {
#define CASE(TYPE, OP)            \
    case TYPE:                    \
        *value = *value OP right; \
        break
            CASE(Token::LessThan, <);
            CASE(Token::LessEqual, <=);
            CASE(Token::GreaterThan, >);
            CASE(Token::GreaterEqual, >);
            CASE(Token::Equals, ==);
            CASE(Token::NotEquals, !=);
            {
                case Token::Comma:
                    *value = right;
                    break;
            }
            CASE(Token::Plus, +);
            CASE(Token::Minus, -);
            CASE(Token::Divide, /);
            CASE(Token::Star, *);
            CASE(Token::Ampersand, &);
            CASE(Token::And, &&);
            CASE(Token::Pipe, |);
            CASE(Token::Or, ||);
            CASE(Token::Xor, ^);
            CASE(Token::LeftShift, <<);
            CASE(Token::RightShift, >>);
#undef CASE
            default:
                CZ_PANIC("Unimplemented eval_expression operator");
        }
    }
}

static Result process_defined_macro(Context* context,
                                    Preprocessor* preprocessor,
                                    lex::Lexer* lexer,
                                    Token* token) {
    ZoneScoped;

    Location* point = &preprocessor->include_stack.last().location;
    Span defined_span = token->span;
    bool at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point, token,
                         &at_bol)) {
        context->report_lex_error(defined_span, "`defined` must be given a macro to test");
        return {Result::ErrorInvalidInput};
    }
    if (at_bol) {
        context->report_lex_error(defined_span, "`defined` must be given a macro to test");
        return {Result::ErrorInvalidInput};
    }

    bool defined;
    if (token->type == Token::Identifier) {
        defined = preprocessor->definitions.get(token->v.identifier.str, token->v.identifier.hash);
    } else if (token->type == Token::OpenParen) {
        Span open_paren_span = token->span;
        if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                             token, &at_bol)) {
            context->report_lex_error(defined_span, "`defined` must be given a macro to test");
            return {Result::ErrorInvalidInput};
        }
        if (at_bol || token->type != Token::Identifier) {
            context->report_lex_error(defined_span, "`defined` must be given a macro to test");
            return {Result::ErrorInvalidInput};
        }

        defined = preprocessor->definitions.get(token->v.identifier.str, token->v.identifier.hash);

        if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                             token, &at_bol)) {
            context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`) here");
            return {Result::ErrorInvalidInput};
        }
        if (at_bol || token->type != Token::CloseParen) {
            context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`) here");
            return {Result::ErrorInvalidInput};
        }
    } else {
        context->report_lex_error(defined_span, "`defined` must be given a macro to test");
        return {Result::ErrorInvalidInput};
    }

    token->type = Token::Integer;
    token->v.integer.value = defined;
    token->v.integer.suffix = 0;
    return Result::ok();
}

static Result process_if(Context* context,
                         Preprocessor* preprocessor,
                         lex::Lexer* lexer,
                         Token* token) {
    ZoneScoped;

    int64_t value;
    {
        Span if_span = token->span;

        // Todo: make this more efficient by not storing all the tokens.
        cz::Vector<Token> tokens = {};
        CZ_DEFER(tokens.drop(context->temp_buffer_array.allocator()));
        tokens.reserve(context->temp_buffer_array.allocator(), 8);

        Include_Info* point;
        while (1) {
            Result ntid_result =
                next_token_in_definition(context, preprocessor, lexer, token, true, 1);
            if (ntid_result.type == Result::Done) {
                bool at_bol = false;
                point = &preprocessor->include_stack.last();
                Location backup_point = point->location;
                if (!lex::next_token(context, lexer,
                                     context->files.files[point->location.file].contents,
                                     &point->location, token, &at_bol)) {
                    break;
                }
                if (at_bol) {
                    point->location = backup_point;
                    break;
                }
            }

        retest:
            if (token->type == Token::Identifier) {
                if (token->v.identifier.str == "defined") {
                    CZ_TRY(process_defined_macro(context, preprocessor, lexer, token));
                    goto add_token;
                }

                if (preprocessor->definition_stack.len() > 0) {
                    // This identifier is either undefined or currently expanded and treated as
                    // undefined because otherwise the ntid would've eaten it.
                    goto undefined_identifier;
                }

                Definition* definition;
                definition = preprocessor->definitions.get(token->v.identifier.str,
                                                           token->v.identifier.hash);
                if (definition) {
                    Result pdi_result = process_defined_identifier(context, preprocessor, lexer,
                                                                   token, definition, true);
                    if (pdi_result.is_err()) {
                        return pdi_result;
                    } else if (pdi_result.type == Result::Done) {
                        // Empty definition.
                        continue;
                    } else {
                        goto retest;
                    }
                } else {
                undefined_identifier:
                    // According to the C standard, undefined tokens are converted to 0.
                    token->type = Token::Integer;
                    token->v.integer.value = 0;
                    token->v.integer.suffix = 0;
                }
            }

        add_token:
            tokens.reserve(context->temp_buffer_array.allocator(), 1);
            tokens.push(*token);
        }

        point->if_stack.reserve(cz::heap_allocator(), 1);
        point->if_stack.push(if_span);

        size_t index = 0;
        if (index == tokens.len()) {
            context->report_lex_error(if_span, "No expression to test");
            return {Result::ErrorInvalidInput};
        }

        CZ_TRY(parse_and_eval_expression(context, if_span, tokens, &index, &value, 100));

        if (index < tokens.len()) {
            CZ_DEBUG_ASSERT(tokens[index].type == Token::CloseParen);
            context->report_error(tokens[index].span, if_span,
                                  "Unmatched closing parenthesis (`)`)");
            return {Result::ErrorInvalidInput};
        }
    }

    if (value) {
        return process_if_true(context, preprocessor, lexer, token);
    } else {
        return process_if_false(context, preprocessor, lexer, token, true, true);
    }
}

static Result process_else(Context* context,
                           Preprocessor* preprocessor,
                           lex::Lexer* lexer,
                           Token* token) {
    ZoneScoped;
    // We just produced x and are skipping over y
    // #if 1
    // x
    // |#else
    // y
    // #endif

    Include_Info* point = &preprocessor->include_stack.last();
    if (point->if_stack.len() == 0) {
        context->report_lex_error(token->span, "#else without #if");
        return {Result::ErrorInvalidInput};
    }

    return process_if_false(context, preprocessor, lexer, token, false, true);
}

static Result process_elif(Context* context,
                           Preprocessor* preprocessor,
                           lex::Lexer* lexer,
                           Token* token) {
    ZoneScoped;
    // We just produced x and are skipping over y and z
    // #if 1
    // x
    // |#elif 2
    // y
    // #else
    // z
    // #endif

    Include_Info* point = &preprocessor->include_stack.last();
    if (point->if_stack.len() == 0) {
        context->report_lex_error(token->span, "#else without #if");
        return {Result::ErrorInvalidInput};
    }

    return process_if_false(context, preprocessor, lexer, token, false, false);
}

static Result process_endif(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token) {
    ZoneScoped;
    Include_Info* point = &preprocessor->include_stack.last();
    if (point->if_stack.len() == 0) {
        context->report_lex_error(token->span, "#endif without #if");
        return {Result::ErrorInvalidInput};
    }

    point->if_stack.pop();
    return SKIP_UNTIL_EOL();
}

static Result process_define(Context* context,
                             Preprocessor* preprocessor,
                             lex::Lexer* lexer,
                             Token* token) {
    ZoneScoped;
    Span define_span = token->span;

    Location* point = &preprocessor->include_stack.last().location;
    bool at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point, token,
                         &at_bol)) {
        context->report_lex_error(define_span, "Must give the macro a name");
        return next_token(context, preprocessor, lexer, token);
    }
    if (at_bol) {
        context->report_lex_error(define_span, "Must give the macro a name");
        return process_token(context, preprocessor, lexer, token, at_bol);
    }
    if (token->type != Token::Identifier) {
        context->report_lex_error(token->span, "Must give the macro a name");
        return SKIP_UNTIL_EOL();
    }

    Hashed_Str identifier = token->v.identifier;
    Location identifier_end = token->span.end;

    Definition definition = {};
    definition.is_function = false;

    at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point, token,
                         &at_bol)) {
        at_bol = false;
        goto end_definition;
    }
    if (at_bol) {
        goto end_definition;
    }

    {
        // We map the parameters names to indexes and then look them up while parsing the body of
        // the macro.  This makes macro expansion much simpler and faster, which is the more common
        // case.
        cz::Str_Map<size_t> parameters = {};
        CZ_DEFER(parameters.drop(cz::heap_allocator()));

        if (token->span.start == identifier_end && token->type == Token::OpenParen) {
            // Functional macro
            Span open_paren_span = token->span;
            if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                                 token, &at_bol)) {
                context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                return next_token(context, preprocessor, lexer, token);
            }
            if (at_bol) {
                context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                return process_token(context, preprocessor, lexer, token, at_bol);
            }

            definition.has_varargs = false;

            // We have to do this crazy thing to deal with erroring on trailing `,`s.
            // Basically in `(a, b, c)` we are at `a` and need to process it then continue into `,`
            // and identifier pairs.
            if (token->type == Token::Identifier) {
                parameters.reserve(cz::heap_allocator(), 1);
                parameters.insert(token->v.identifier.str, token->v.identifier.hash,
                                  parameters.count);
                while (1) {
                    // In `(a, b, c)`, we are at `,` or `)`.
                    if (!lex::next_token(context, lexer, context->files.files[point->file].contents,
                                         point, token, &at_bol)) {
                        context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                        return next_token(context, preprocessor, lexer, token);
                    }
                    if (at_bol) {
                        context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                        return process_token(context, preprocessor, lexer, token, at_bol);
                    }

                    if (token->type == Token::CloseParen) {
                        break;
                    }
                    if (definition.has_varargs) {
                        context->report_lex_error(
                            open_paren_span, "Varargs specifier (`...`) must be last parameter");
                        return SKIP_UNTIL_EOL();
                    }
                    if (token->type != Token::Comma) {
                        context->report_lex_error(open_paren_span,
                                                  "Must have comma between parameters");
                        return SKIP_UNTIL_EOL();
                    }

                    // In `(a, b, c)`, we are at `b` or `c`.
                    if (!lex::next_token(context, lexer, context->files.files[point->file].contents,
                                         point, token, &at_bol)) {
                        context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                        return next_token(context, preprocessor, lexer, token);
                    }
                    if (at_bol) {
                        context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                        return process_token(context, preprocessor, lexer, token, at_bol);
                    }

                    if (token->type == Token::Identifier) {
                        parameters.reserve(cz::heap_allocator(), 1);
                        if (parameters.get(token->v.identifier.str, token->v.identifier.hash)) {
                            context->report_lex_error(token->span, "Parameter already used");
                            return SKIP_UNTIL_EOL();
                        }
                        parameters.insert(token->v.identifier.str, token->v.identifier.hash,
                                          parameters.count);
                    } else if (token->type == Token::Preprocessor_Varargs_Parameter_Indicator) {
                        definition.has_varargs = true;
                    } else {
                        context->report_lex_error(token->span, "Must have parameter name here");
                        return SKIP_UNTIL_EOL();
                    }
                }
            } else if (token->type == Token::Preprocessor_Varargs_Parameter_Indicator) {
                definition.has_varargs = true;

                if (!lex::next_token(context, lexer, context->files.files[point->file].contents,
                                     point, token, &at_bol)) {
                    context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                    return next_token(context, preprocessor, lexer, token);
                }
                if (at_bol) {
                    context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                    return process_token(context, preprocessor, lexer, token, at_bol);
                }

                if (token->type != Token::CloseParen) {
                    context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                    return SKIP_UNTIL_EOL();
                }
            } else if (token->type == Token::CloseParen) {
            } else {
                context->report_lex_error(open_paren_span, "Unpaired parenthesis (`(`)");
                return SKIP_UNTIL_EOL();
            }

            definition.parameter_len = parameters.count;
            definition.is_function = true;
        } else {
            definition.tokens.reserve(cz::heap_allocator(), 1);
            definition.tokens.push(*token);
        }

        // Process the definition body.
        while (1) {
            if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                                 token, &at_bol)) {
                at_bol = false;
                break;
            }
            if (at_bol) {
                break;
            }

            // If the token matches a parameter, mark it as a parameter and replace its string value
            // with its index.
            if (token->type == Token::Identifier) {
                uint64_t* parameter =
                    parameters.get(token->v.identifier.str, token->v.identifier.hash);
                if (parameter) {
                    token->type = Token::Preprocessor_Parameter;
                    token->v.integer.value = *parameter;
                } else if (token->v.identifier.str == "__VAR_ARGS__") {
                    token->type = Token::Preprocessor_Parameter;
                    token->v.integer.value = parameters.count;
                }
            } else if (token->type == Token::HashHash) {
                if (definition.tokens.len() == 0) {
                    // :ConcatErrors ## errors are assumed to be eliminated in
                    // next_token_in_definition
                    context->report_lex_error(
                        token->span, "Token concatenation (`##`) must have a token before it");
                    continue;
                }
            }

            definition.tokens.reserve(cz::heap_allocator(), 1);
            definition.tokens.push(*token);
        }

        if (definition.tokens.len() > 0) {
            Token* last_token = &definition.tokens.last();
            if (last_token->type == Token::HashHash) {
                // :ConcatErrors ## errors are assumed to be eliminated in next_token_in_definition
                context->report_lex_error(last_token->span,
                                          "Token concatenation (`##`) must have a token after it");
                definition.tokens.pop();
            }
        }

        // kill parameters
    }

end_definition:
    preprocessor->definitions.reserve(cz::heap_allocator(), 1);

    Definition* dd = preprocessor->definitions.get(identifier.str, identifier.hash);
    if (dd) {
        dd->drop(cz::heap_allocator());
        *dd = definition;
    } else {
        preprocessor->definitions.insert(identifier.str, identifier.hash, definition);
    }

    return process_next(context, preprocessor, lexer, token, at_bol, at_bol);
}

static Result process_undef(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token) {
    ZoneScoped;
    Span undef_span = token->span;

    Include_Info* point = &preprocessor->include_stack.last();
    bool at_bol = false;
    if (!lex::next_token(context, lexer, context->files.files[point->location.file].contents,
                         &point->location, token, &at_bol)) {
        context->report_lex_error(undef_span, "Must specify the macro to undefine");
        return next_token(context, preprocessor, lexer, token);
    }
    if (at_bol) {
        context->report_lex_error(undef_span, "Must specify the macro to undefine");
        return process_token(context, preprocessor, lexer, token, at_bol);
    }
    if (token->type != Token::Identifier) {
        context->report_lex_error(token->span, "Must specify the macro to undefine");
        return SKIP_UNTIL_EOL();
    }

    size_t index;
    if (preprocessor->definitions.index_of(token->v.identifier.str, token->v.identifier.hash,
                                           &index)) {
        preprocessor->definitions.values[index].drop(cz::heap_allocator());
        preprocessor->definitions.set_tombstone(index);
    }

    return SKIP_UNTIL_EOL();
}

static Result process_error(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token) {
    ZoneScoped;
    context->report_lex_error(token->span, "Explicit error");
    return SKIP_UNTIL_EOL();
}

static Result process_defined_identifier(Context* context,
                                         Preprocessor* preprocessor,
                                         lex::Lexer* lexer,
                                         Token* token,
                                         Definition* definition,
                                         bool this_line_only) {
    ZoneScoped;
    preprocessor->definition_stack.reserve(cz::heap_allocator(), 1);

    Definition_Info info = {};
    info.definition = definition;

    if (definition->is_function) {
        // Process arguments.
        Token identifier_token = *token;
        Location* point = &preprocessor->include_stack.last().location;
        Location backup_point_open_paren = *point;

        // We expect an open parenthesis here.  If there isn't one, just don't expand the macro.
        bool at_bol = false;
        Result ntid_result =
            next_token_in_definition(context, preprocessor, lexer, token, this_line_only, 0);
        if (ntid_result.type == Result::Done) {
            if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                                 token, &at_bol)) {
                *point = backup_point_open_paren;
                *token = identifier_token;
                return Result::ok();
            }
            if (token->type != Token::OpenParen || (at_bol && this_line_only)) {
                *point = backup_point_open_paren;
                *token = identifier_token;
                return Result::ok();
            }
        }

        Span open_paren_span = token->span;

        // Get the first token of the body.  After this point no more tokens form errors.
        at_bol = false;
        ntid_result =
            next_token_in_definition(context, preprocessor, lexer, token, this_line_only, 0);
        if (ntid_result.type == Result::Done) {
            if (!lex::next_token(context, lexer, context->files.files[point->file].contents, point,
                                 token, &at_bol)) {
                context->report_error(open_paren_span, source_span(preprocessor),
                                      "Unpaired parenthesis (`(`)");
                return {Result::ErrorInvalidInput};
            }
            if (at_bol && this_line_only) {
                context->report_error(open_paren_span, source_span(preprocessor),
                                      "Unpaired parenthesis (`(`)");
                return {Result::ErrorInvalidInput};
            }
        }

        // Process arguments
        cz::Vector<Token> argument_tokens = {};
        size_t paren_depth = 0;
        while (1) {
            if (token->type == Token::OpenParen) {
                ++paren_depth;
            } else if (token->type == Token::CloseParen) {
                if (paren_depth > 0) {
                    --paren_depth;
                } else {
                    // If we read "()" and there are no parameters, then skip adding an argument.
                    // However if there is a parameter and we read "()" then we add an empty
                    // argument.
                    if (info.arguments.len() != 0 || argument_tokens.len() != 0 ||
                        definition->parameter_len != 0) {
                        argument_tokens.realloc(cz::heap_allocator());
                        info.arguments.reserve(cz::heap_allocator(), 1);
                        info.arguments.push(argument_tokens);
                    }

                    if (info.arguments.len() < definition->parameter_len) {
                        context->report_error(open_paren_span, source_span(preprocessor),
                                              "Too few arguments to macro (expected ",
                                              definition->parameter_len, ")");
                        return {Result::ErrorInvalidInput};
                    }
                    if (info.arguments.len() >
                        definition->parameter_len + definition->has_varargs) {
                        context->report_error(open_paren_span, source_span(preprocessor),
                                              "Too many arguments to macro (expected ",
                                              definition->parameter_len, ")");
                        return {Result::ErrorInvalidInput};
                    }

                    if (info.arguments.len() == definition->parameter_len &&
                        definition->has_varargs) {
                        info.arguments.reserve(cz::heap_allocator(), 1);
                        info.arguments.push({});
                    }

                    info.arguments.realloc(cz::heap_allocator());
                    goto do_expand;
                }
            } else if (paren_depth == 0 && token->type == Token::Comma) {
                if (definition->has_varargs && info.arguments.len() == definition->parameter_len) {
                    goto append_argument_token;
                }

                argument_tokens.realloc(cz::heap_allocator());
                info.arguments.reserve(cz::heap_allocator(), 1);
                info.arguments.push(argument_tokens);
                argument_tokens = {};
                goto next_argument_token;
            }

        append_argument_token:
            argument_tokens.reserve(cz::heap_allocator(), 1);
            argument_tokens.push(*token);

        next_argument_token:
            ntid_result =
                next_token_in_definition(context, preprocessor, lexer, token, this_line_only, 0);
            if (ntid_result.type == Result::Done) {
                if (!lex::next_token(context, lexer, context->files.files[point->file].contents,
                                     point, token, &at_bol)) {
                    context->report_error(open_paren_span, source_span(preprocessor),
                                          "Unpaired parenthesis (`(`)");
                    return {Result::ErrorInvalidInput};
                }
                if (at_bol && this_line_only) {
                    context->report_error(open_paren_span, source_span(preprocessor),
                                          "Unpaired parenthesis (`(`)");
                    return {Result::ErrorInvalidInput};
                }
            }
        }
    }

do_expand:
    preprocessor->definition_stack.push(info);
    return next_token(context, preprocessor, lexer, token);
}

static Result process_identifier(Context* context,
                                 Preprocessor* preprocessor,
                                 lex::Lexer* lexer,
                                 Token* token) {
    ZoneScoped;
    Definition* definition =
        preprocessor->definitions.get(token->v.identifier.str, token->v.identifier.hash);
    if (definition) {
        return process_defined_identifier(context, preprocessor, lexer, token, definition, false);
    } else {
        return Result::ok();
    }
}

static Result process_token(Context* context,
                            Preprocessor* preprocessor,
                            lex::Lexer* lexer,
                            Token* token,
                            bool at_bol) {
    ZoneScoped;
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
                if (token->v.identifier.str == "if") {
                    return process_if(context, preprocessor, lexer, token);
                }
                if (token->v.identifier.str == "else") {
                    return process_else(context, preprocessor, lexer, token);
                }
                if (token->v.identifier.str == "elif") {
                    return process_elif(context, preprocessor, lexer, token);
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
            }

            context->report_lex_error(token->span, "Unknown preprocessor directive");
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
        return next_token(context, preprocessor, lexer, token);
    }
}

static Result next_token_in_definition(Context* context,
                                       Preprocessor* preprocessor,
                                       lex::Lexer* lexer,
                                       Token* token,
                                       bool this_line_only,
                                       int expand_macros) {
    ZoneScoped;

    /// The theoretical behavior of invoking a functional macro `#define f(x, y, z)` are as follows:
    /// `f(a, b, c)`.  "a", "b", and "c" are stored as 'token soup' as arguments.  That is, they are
    /// unparsed tokens passed to `f`.  When `f` is expanding and sees a parameter being used, it
    /// parses the parameter and expands it fully, bypassing token soupization.  If it sees `#x` it
    /// will only parse `x` and not expand it.
    ///
    /// `expand_macros` allows us to implement this:
    /// * `1` means the token is expanded.  This is the default value.
    /// * `0` is used when expanding function macros and means the arguments are unparsed and
    ///   unexpanded, except for parameters, which are fully parsed and expanded.
    /// * `-1` is used when expanding `#x` where `x` is a parameter.  It means the parameter is
    ///   expanded but macros are not.
    ///
    /// Example:
    /// ```
    /// #define stringify1(x) stringify2(x)
    /// #define stringify2(x) #x
    /// #define a 13
    /// stringify1(a);
    /// stringify2(a);
    /// ```
    ///
    /// In `stringify2(a)`, `#x` would expand the parameter but not expand macros, creating `"a"`.
    ///
    /// In `stringify1(a)`, the call to `stringify2(x)` allows `x` to be fully expanded as `x` is a
    /// parameter and `0` is used in parsing function macro invocations.  Then `stringify2(13)` is
    /// invoked, which trivially evaluates to `"13"`.

    while (preprocessor->definition_stack.len() > 0) {
        Definition_Info* info = &preprocessor->definition_stack.last();
        if (info->index == info->definition->tokens.len()) {
            // This definition has ran through all its tokens.
            Definition_Info info = preprocessor->definition_stack.pop();
            drop(&info);
            continue;
        }

        Token* tk = &info->definition->tokens[info->index];
        if (tk->type == Token::Preprocessor_Parameter) {
            // Run through the tokens in the argument.
            cz::Slice<Token> argument_tokens = info->arguments[tk->v.integer.value];
            if (info->argument_index == argument_tokens.len) {
                // We're at the end of this argument.
                ++info->index;
                info->argument_index = 0;
                continue;
            }
            *token = argument_tokens[info->argument_index++];

            if (expand_macros != -1 && info->argument_index == argument_tokens.len) {
                // We're at the end of this argument.
                ++info->index;
                info->argument_index = 0;
            }

            if (expand_macros == 0) {
                expand_macros = 1;
            }
        } else {
            ++info->index;
            // Since we postincrement index, we need to set `argument_index` to 0 if the next token
            // is a parameter token.  Since we don't use the value for any other reason and a branch
            // is expensive, we just always set it to 0.
            info->argument_index = 0;
            *token = *tk;

            if (token->type == Token::Hash && info->index < info->definition->tokens.len()) {
                if (info->definition->tokens[info->index].type == Token::Preprocessor_Parameter) {
                    // Todo: use lex buffer array directly since we don't lex more tokens while in
                    // the definition stack.  At some point this might change in order to implement
                    // # correctly by replacing arguments with some sort of "token soup" that are
                    // then reparsed each time they are used.  Who knows.
                    cz::AllocatedString string = {};
                    string.allocator = cz::heap_allocator();
                    CZ_DEFER(string.drop());

                    tk = &info->definition->tokens[info->index];
                    size_t pdsl = preprocessor->definition_stack.len();
                    while (1) {
                        Definition_Info* info = &preprocessor->definition_stack.last();
                        if (info->index == info->definition->tokens.len()) {
                            // This definition has ran through all its tokens.
                            Definition_Info info = preprocessor->definition_stack.pop();
                            drop(&info);
                            continue;
                        }

                        if (preprocessor->definition_stack.len() == pdsl) {
                            if (info->argument_index ==
                                info->arguments[tk->v.integer.value].len()) {
                                ++info->index;
                                info->argument_index = 0;
                                break;
                            }
                        }

                        CZ_TRY(next_token_in_definition(context, preprocessor, lexer, token,
                                                        this_line_only, -1));

                        if (string.len() > 0) {
                            // Todo make this not happen all the time
                            write(string_writer(&string), ' ');
                        }
                        write(string_writer(&string), *token);
                    }

                    token->type = Token::String;
                    token->v.string = string.clone(lexer->string_buffer_array.allocator());
                    return Result::ok();
                } else {
                    cz::AllocatedString string = {};
                    string.allocator = lexer->string_buffer_array.allocator();
                    write(string_writer(&string), info->definition->tokens[info->index]);

                    token->type = Token::String;
                    token->v.string = string;
                    ++info->index;
                    return Result::ok();
                }
            }
        }

        if (token->type == Token::HashHash ||
            (info->index < info->definition->tokens.len() &&
             info->definition->tokens[info->index].type == Token::HashHash)) {
            if (token->type != Token::HashHash) {
                ++info->index;
            }

            // :ConcatErrors ## errors are eliminated in process_define
            CZ_DEBUG_ASSERT(info->index < info->definition->tokens.len());

            cz::AllocatedString combined_identifier = {};
            combined_identifier.allocator = lexer->identifier_buffer_array.allocator();

            if (token->type != Token::HashHash) {
                write(string_writer(&combined_identifier), *token);
            }

            while (1) {
                Token* tk = &info->definition->tokens[info->index];
                if (tk->type == Token::Preprocessor_Parameter) {
                    // Get the first token in the argument and then stop chaining.
                    cz::Slice<Token> argument_tokens = info->arguments[tk->v.integer.value];
                    if (info->argument_index == argument_tokens.len) {
                        // We're at the end of this argument.
                        ++info->index;
                        info->argument_index = 0;
                        break;
                    }

                    write(string_writer(&combined_identifier),
                          argument_tokens[info->argument_index]);
                    ++info->argument_index;

                    if (info->argument_index == argument_tokens.len) {
                        ++info->index;
                        info->argument_index = 0;
                    } else {
                        break;
                    }
                } else {
                    write(string_writer(&combined_identifier), *tk);
                    ++info->index;
                    info->argument_index = 0;
                }

                // Deal with chain x ## y ## z
                if (info->index + 1 >= info->definition->tokens.len()) {
                    break;
                } else {
                    tk = &info->definition->tokens[info->index];
                    if (tk->type != Token::HashHash) {
                        break;
                    }
                    ++info->index;
                }
            }

            token->type = Token::Identifier;
            token->v.identifier = Hashed_Str::from_str(combined_identifier);
            return Result::ok();
        }

        if (token->type == Token::Identifier && expand_macros == 1) {
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
            return process_defined_identifier(context, preprocessor, lexer, token, definition,
                                              this_line_only);
        }

        return Result::ok();
    }

    return Result::done();
}

Result next_token(Context* context, Preprocessor* preprocessor, lex::Lexer* lexer, Token* token) {
    ZoneScoped;

    Result result = next_token_in_definition(context, preprocessor, lexer, token, false, true);
    if (result.type != Result::Done) {
        return result;
    }

    if (preprocessor->include_stack.len() == 0) {
        return Result::done();
    }

    Location* point = &preprocessor->include_stack.last().location;
    while (point->index == context->files.files[point->file].contents.len) {
        Include_Info entry = preprocessor->include_stack.pop();

        for (size_t i = 0; i < entry.if_stack.len(); ++i) {
            context->report_lex_error(entry.if_stack[i], "Unterminated #if");
        }

        entry.if_stack.drop(cz::heap_allocator());

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

namespace cpp {

Result next_token(Context* context,
                  pre::Preprocessor* preprocessor,
                  lex::Lexer* lexer,
                  Token* token) {
    Result result = pre::next_token(context, preprocessor, lexer, token);
    if (result.type == Result::Success && token->type == Token::Identifier) {
        ZoneScopedN("cpp::next_token keyword");

        struct Keyword {
            cz::Str value;
            Token::Type type;
        };
        Keyword keywords[] = {
            {"auto", Token::Auto},         {"break", Token::Break},
            {"case", Token::Case},         {"char", Token::Char},
            {"const", Token::Const},       {"continue", Token::Continue},
            {"default", Token::Default},   {"do", Token::Do},
            {"double", Token::Double},     {"else", Token::Else},
            {"enum", Token::Enum},         {"extern", Token::Extern},
            {"float", Token::Float},       {"for", Token::For},
            {"goto", Token::Goto},         {"if", Token::If},
            {"int", Token::Int},           {"long", Token::Long},
            {"register", Token::Register}, {"return", Token::Return},
            {"short", Token::Short},       {"signed", Token::Signed},
            {"sizeof", Token::Sizeof},     {"static", Token::Static},
            {"struct", Token::Struct},     {"switch", Token::Switch},
            {"typedef", Token::Typedef},   {"union", Token::Union},
            {"unsigned", Token::Unsigned}, {"void", Token::Void},
            {"volatile", Token::Volatile}, {"while", Token::While},
        };

        cz::Str value = token->v.identifier.str;
        for (size_t i = 0; i < sizeof(keywords) / sizeof(*keywords); ++i) {
            if (value == keywords[i].value) {
                token->type = keywords[i].type;
                return Result::ok();
            }
        }
    }
    return result;
}

}
}
