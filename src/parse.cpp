#include "parse.hpp"

#include <cz/heap.hpp>
#include <cz/try.hpp>
#include <new>
#include "context.hpp"
#include "result.hpp"

namespace red {
namespace parse {

static Type* make_primitive(cz::Allocator allocator, Type::Tag tag) {
    Type* type = allocator.alloc<Type>();
    new (type) Type(tag);
    return type;
}

void Parser::init() {
    lexer.init();
    buffer_array.create();
    type_char = make_primitive(buffer_array.allocator(), Type::Builtin_Char);
    type_double = make_primitive(buffer_array.allocator(), Type::Builtin_Double);
    type_float = make_primitive(buffer_array.allocator(), Type::Builtin_Float);
    type_int = make_primitive(buffer_array.allocator(), Type::Builtin_Int);
    type_long = make_primitive(buffer_array.allocator(), Type::Builtin_Long);
    type_short = make_primitive(buffer_array.allocator(), Type::Builtin_Short);
    type_void = make_primitive(buffer_array.allocator(), Type::Builtin_Void);
    back.type = Token::Parser_Null_Token;

    type_stack.reserve(cz::heap_allocator(), 4);
    typedef_stack.reserve(cz::heap_allocator(), 4);
    declaration_stack.reserve(cz::heap_allocator(), 4);
    type_stack.push({});
    typedef_stack.push({});
    declaration_stack.push({});
}

void Parser::drop() {
    for (size_t i = 0; i < type_stack.len(); ++i) {
        type_stack[i].drop(cz::heap_allocator());
    }
    type_stack.drop(cz::heap_allocator());

    for (size_t i = 0; i < typedef_stack.len(); ++i) {
        typedef_stack[i].drop(cz::heap_allocator());
    }
    typedef_stack.drop(cz::heap_allocator());

    for (size_t i = 0; i < declaration_stack.len(); ++i) {
        declaration_stack[i].drop(cz::heap_allocator());
    }
    declaration_stack.drop(cz::heap_allocator());

    buffer_array.drop();
    preprocessor.destroy();
    lexer.drop();
}

static Result next_token(Context* context, Parser* parser, Token* token) {
    if (parser->back.type != Token::Parser_Null_Token) {
        *token = parser->back;
        parser->back.type = Token::Parser_Null_Token;
        return Result::ok();
    }
    return cpp::next_token(context, &parser->preprocessor, &parser->lexer, token);
}

static Result peek_token(Context* context, Parser* parser, Token* token) {
    if (parser->back.type != Token::Parser_Null_Token) {
        *token = parser->back;
        return Result::ok();
    }
    Result result = cpp::next_token(context, &parser->preprocessor, &parser->lexer, token);
    if (result.is_ok()) {
        parser->back = *token;
    }
    return result;
}

static Declaration* lookup_declaration(Parser* parser, Hashed_Str id) {
    for (size_t i = parser->declaration_stack.len(); i-- > 0;) {
        Declaration* declaration = parser->declaration_stack[i].get(id.str, id.hash);
        if (declaration) {
            return declaration;
        }
    }
    return nullptr;
}

static TypeP* lookup_typedef(Parser* parser, Hashed_Str id) {
    for (size_t i = parser->typedef_stack.len(); i-- > 0;) {
        TypeP* type = parser->typedef_stack[i].get(id.str, id.hash);
        if (type) {
            return type;
        }
    }
    return nullptr;
}

static Type** lookup_type(Parser* parser, Hashed_Str id) {
    for (size_t i = parser->type_stack.len(); i-- > 0;) {
        Type** type = parser->type_stack[i].get(id.str, id.hash);
        if (type) {
            return type;
        }
    }
    return nullptr;
}

static bool parse_type_qualifier(Context* context, TypeP* type, Token token) {
    switch (token.type) {
        case Token::Const:
            if (type->is_const()) {
                context->report_error(token.span, "Multiple `const` attributes");
            }
            type->set_const();
            return true;

        case Token::Volatile:
            if (type->is_volatile()) {
                context->report_error(token.span, "Multiple `volatile` attributes");
            }
            type->set_volatile();
            return true;

        default:
            return false;
    }
}

static Result parse_declaration_after_identifier(Context* context,
                                                 Parser* parser,
                                                 Declaration* declaration,
                                                 Hashed_Str identifier,
                                                 Token* token) {
    // Todo: support arrays
    Span previous_span = token->span;
    Result result = next_token(context, parser, token);
    CZ_TRY_VAR(result);
    if (result.type == Result::Done) {
        context->report_error(previous_span, "Expected ';' to end declaration here");
        return {Result::ErrorInvalidInput};
    }

    if (token->type == Token::Set) {
        // Eat the value.
        result = parse_expression(context, parser, &declaration->o_value);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(previous_span, "Expected ';' to end declaration here");
            return {Result::ErrorInvalidInput};
        }

        // Then eat `;` or `,` after the value.
        previous_span = token->span;
        result = next_token(context, parser, token);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(previous_span, "Expected ';' to end declaration here");
            return {Result::ErrorInvalidInput};
        }
    } else {
        declaration->o_value = nullptr;
    }

    cz::Str_Map<Declaration>* declarations = &parser->declaration_stack.last();
    if (!declarations->get(identifier.str, identifier.hash)) {
        declarations->reserve(cz::heap_allocator(), 1);
        declarations->insert(identifier.str, identifier.hash, *declaration);
    } else {
        context->report_error(previous_span, "Declaration with same name also in scope");
        return {Result::ErrorInvalidInput};
    }

    if (token->type == Token::Comma) {
        return Result::ok();
    } else if (token->type == Token::Semicolon) {
        return {Result::Done};
    } else {
        context->report_error(previous_span, "Expected ';' to end declaration here");
        return {Result::ErrorInvalidInput};
    }
}

static Result parse_declaration_after_base_type(Context* context,
                                                Parser* parser,
                                                TypeP base_type,
                                                Token* token) {
    Declaration declaration = {};
    declaration.type = base_type;

    bool allow_qualifiers = false;
    while (1) {
        Span previous_span = token->span;
        Result result = next_token(context, parser, token);
        CZ_TRY_VAR(result);
        if (result.type != Result::Success) {
            context->report_error(previous_span, "Expected ';' to end declaration here");
            return {Result::ErrorInvalidInput};
        }
        previous_span = token->span;

        if (allow_qualifiers && parse_type_qualifier(context, &declaration.type, *token)) {
            continue;
        }

        switch (token->type) {
            case Token::Star: {
                Type_Pointer* pointer = parser->buffer_array.allocator().create<Type_Pointer>();
                pointer->inner = declaration.type;
                declaration.type.clear();
                declaration.type.set_type(pointer);
                allow_qualifiers = true;
                break;
            }

            case Token::Identifier:
                return parse_declaration_after_identifier(context, parser, &declaration,
                                                          token->v.identifier, token);

            default:
                context->report_error(previous_span,
                                      "Expected identifier here to complete declaration");
                return {Result::ErrorInvalidInput};
        }
    }
}

static Result parse_base_type(Context* context, Parser* parser, TypeP* base_type) {
    Token token;
    Result result;
    while (1) {
        Span span = token.span;
        result = next_token(context, parser, &token);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(span, "No type to make `",
                                  token.type == Token::Const ? "const" : "volatile", "`");
            return {Result::ErrorInvalidInput};
        }

        if (!parse_type_qualifier(context, base_type, token)) {
            break;
        }
    }

    switch (token.type) {
        case Token::Identifier: {
            Declaration* declaration = lookup_declaration(parser, token.v.identifier);
            TypeP* type = lookup_typedef(parser, token.v.identifier);
            if (declaration) {
                if (type) {
                    Type* t = type->get_type();
                    if (t->tag == Type::Enum) {
                        context->report_error(
                            token.span,
                            "Variable cannot be used as a type.  Hint: add the tag `enum`");
                    } else if (t->tag == Type::Struct) {
                        context->report_error(
                            token.span,
                            "Variable cannot be used as a type.  Hint: add the tag `struct`");
                    } else if (t->tag == Type::Union) {
                        context->report_error(
                            token.span,
                            "Variable cannot be used as a type.  Hint: add the tag `union`");
                    } else {
                        // Todo: add hint about spelling out the type
                        context->report_error(token.span, "Variable cannot be used as a type.");
                    }
                } else {
                    context->report_error(token.span, "Variable cannot be used as a type");
                }
                return {Result::ErrorInvalidInput};
            }

            if (!type) {
                context->report_error(token.span, "Undefined type `", token.v.identifier.str, "`");
                return {Result::ErrorInvalidInput};
            }

            base_type->merge_typedef(*type);
            break;
        }

        case Token::Char:
            base_type->set_type(parser->type_char);
            break;
        case Token::Double:
            base_type->set_type(parser->type_double);
            break;
        case Token::Float:
            base_type->set_type(parser->type_float);
            break;
        case Token::Int:
            base_type->set_type(parser->type_int);
            break;
        case Token::Long:
            base_type->set_type(parser->type_long);
            break;
        case Token::Short:
            base_type->set_type(parser->type_short);
            break;
        case Token::Void:
            base_type->set_type(parser->type_void);
            break;

        default:
            context->report_error(token.span, "Expected type here");
            return {Result::ErrorInvalidInput};
    }

    while (1) {
        result = next_token(context, parser, &token);
        if (result.type != Result::Success) {
            return result;
        }

        if (parse_type_qualifier(context, base_type, token)) {
            continue;
        }

        parser->back = token;
        break;
    }

    return Result::ok();
}

Result parse_declaration(Context* context, Parser* parser) {
    // Parse base type and then parse a series of declarations.
    // Example 1: int const a, b;
    //            base_type = const int
    //            declaration 1 = a
    //            declaration 2 = b
    // Example 2: int *a, b;
    //            base_type = int
    //            declaration 1 = *a
    //            declaration 2 = b
    TypeP base_type = {};
    CZ_TRY(parse_base_type(context, parser, &base_type));

    Token token;
    Result result = peek_token(context, parser, &token);
    CZ_TRY_VAR(result);
    if (result.is_ok() && token.type == Token::Semicolon) {
        return Result::ok();
    }

    while (1) {
        result = parse_declaration_after_base_type(context, parser, base_type, &token);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            break;
        }
    }

    return Result::ok();
}

Result parse_expression_(Context* context, Parser* parser, Expression** eout, int max_precedence) {
    Token token;
    Result result = next_token(context, parser, &token);
    if (result.type != Result::Success) {
        return result;
    }

    // Parse one atom
    switch (token.type) {
        case Token::Integer: {
            Expression_Integer* expression =
                parser->buffer_array.allocator().create<Expression_Integer>();
            expression->value = token.v.integer.value;
            *eout = expression;
            break;
        }

        case Token::Identifier: {
            if (lookup_declaration(parser, token.v.identifier)) {
                Expression_Variable* expression =
                    parser->buffer_array.allocator().create<Expression_Variable>();
                expression->variable = token.v.identifier;
                *eout = expression;
                break;
            } else {
                context->report_error(token.span, "Undefined variable `", token.v.identifier.str,
                                      "`");
                return {Result::ErrorInvalidInput};
            }
        }

        case Token::OpenParen: {
            result = parse_expression(context, parser, eout);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(token.span, "Unmatched parenthesis (`(`)");
                return {Result::ErrorInvalidInput};
            }

            Span open_paren_span = token.span;
            result = peek_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(open_paren_span, "Unmatched parenthesis (`(`)");
                return {Result::ErrorInvalidInput};
            }
            if (token.type != Token::CloseParen) {
                context->report_error(token.span, "Expected close parenthesis (`(`) here");
                return {Result::ErrorInvalidInput};
            }
            break;
        }

        default:
            context->report_error(token.span, "Expected expression here");
            return {Result::ErrorInvalidInput};
    }

    while (1) {
        result = peek_token(context, parser, &token);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            return Result::ok();
        }

        int precedence;
        bool ltr = true;
        switch (token.type) {
            case Token::CloseParen:
            case Token::Semicolon:
                return Result::ok();

            case Token::LessThan:
            case Token::LessEqual:
            case Token::GreaterThan:
            case Token::GreaterEqual:
                precedence = 9;
                break;
            case Token::Set:
                precedence = 16;
                ltr = false;
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
            default:
                context->report_error(token.span,
                                      "Expected binary operator here to connect expressions");
                return {Result::ErrorInvalidInput};
        }

        if (precedence >= max_precedence) {
            return Result::ok();
        }

        parser->back.type = Token::Parser_Null_Token;

        Expression* right;
        result = parse_expression_(context, parser, &right, precedence + !ltr);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(token.span, "Expected right side for binary operator here");
            return {Result::ErrorInvalidInput};
        }

        Expression_Binary* binary = parser->buffer_array.allocator().create<Expression_Binary>();
        binary->op = token.type;
        binary->left = *eout;
        binary->right = right;
        *eout = binary;
    }

    return Result::ok();
}

Result parse_expression(Context* context, Parser* parser, Expression** eout) {
    return parse_expression_(context, parser, eout, 100);
}

}
}
