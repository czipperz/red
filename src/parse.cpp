#include "parse.hpp"

#include <cz/defer.hpp>
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

static Result parse_expression_(Context* context,
                                Parser* parser,
                                Expression** eout,
                                int max_precedence);

static Result parse_declaration_after_identifier(Context* context,
                                                 Parser* parser,
                                                 Declaration* declaration,
                                                 Hashed_Str identifier,
                                                 Token* token,
                                                 cz::Vector<Statement*>* initializers) {
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
        Expression* value;
        result = parse_expression_(context, parser, &value, 17);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(previous_span, "Expected ';' to end declaration here");
            return {Result::ErrorInvalidInput};
        }

        Statement_Initializer_Copy* initializer =
            parser->buffer_array.allocator().create<Statement_Initializer_Copy>();
        initializer->identifier = identifier;
        initializer->value = value;
        initializers->reserve(cz::heap_allocator(), 1);
        initializers->push(initializer);

        // Then eat `;` or `,` after the value.
        previous_span = token->span;
        result = next_token(context, parser, token);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(previous_span, "Expected ';' to end declaration here");
            return {Result::ErrorInvalidInput};
        }
    } else {
        Statement_Initializer_Default* initializer =
            parser->buffer_array.allocator().create<Statement_Initializer_Default>();
        initializer->identifier = identifier;
        initializers->reserve(cz::heap_allocator(), 1);
        initializers->push(initializer);
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
                                                Token* token,
                                                cz::Vector<Statement*>* initializers) {
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
                                                          token->v.identifier, token, initializers);

            default:
                context->report_error(previous_span,
                                      "Expected identifier here to complete declaration");
                return {Result::ErrorInvalidInput};
        }
    }
}

static Result parse_composite_body(Context* context,
                                   Parser* parser,
                                   cz::Vector<Statement*>* initializers,
                                   uint32_t* flags,
                                   Span composite_span) {
    while (1) {
        Token token;
        Result result = peek_token(context, parser, &token);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(composite_span, "Expected close curly (`}`) to end struct body");
            return {Result::ErrorInvalidInput};
        }
        if (token.type == Token::CloseCurly) {
            parser->back.type = Token::Parser_Null_Token;
            return Result::ok();
        }

        CZ_TRY(parse_declaration(context, parser, initializers));
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
        case Token::Struct: {
            Span struct_span = token.span;
            result = next_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(struct_span, "Expected struct name, body, or `;` here");
                return {Result::ErrorInvalidInput};
            }

            Span identifier_span;
            Hashed_Str identifier = {};
            if (token.type == Token::Identifier) {
                identifier = token.v.identifier;
                identifier_span = token.span;

                result = next_token(context, parser, &token);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(struct_span,
                                          "Expected declaration, struct body, `;` here");
                    return {Result::ErrorInvalidInput};
                }
            }

            if (token.type == Token::Semicolon) {
                if (identifier.str.len > 0) {
                    Type** type = lookup_type(parser, identifier);
                    if (type) {
                        if ((*type)->tag != Type::Struct) {
                            context->report_error(identifier_span, "Type `", identifier.str,
                                                  "` is not a struct");
                            return {Result::ErrorInvalidInput};
                        }
                    } else {
                        Type_Struct* struct_type =
                            parser->buffer_array.allocator().create<Type_Struct>();
                        struct_type->types = {};
                        struct_type->typedefs = {};
                        struct_type->declarations = {};
                        struct_type->initializers = {};
                        struct_type->flags = 0;
                        cz::Str_Map<Type*>* types = &parser->type_stack.last();
                        types->reserve(cz::heap_allocator(), 1);
                        types->insert(identifier.str, identifier.hash, struct_type);
                    }
                }
                parser->back = token;
                return Result::ok();
            }

            if (token.type == Token::OpenCurly) {
                Type** type = lookup_type(parser, identifier);
                Type_Struct* struct_type = nullptr;
                if (type) {
                    if ((*type)->tag != Type::Struct) {
                        context->report_error(identifier_span, "Type `", identifier.str,
                                              "` is not a struct");
                        return {Result::ErrorInvalidInput};
                    }
                    struct_type = (Type_Struct*)*type;
                    if (struct_type->flags & Composite_Flags::Defined) {
                        context->report_error(identifier_span, "Type `", identifier.str,
                                              "` is already defined");
                        return {Result::ErrorInvalidInput};
                    }
                }

                uint32_t flags = Composite_Flags::Defined;

                parser->type_stack.reserve(cz::heap_allocator(), 1);
                parser->typedef_stack.reserve(cz::heap_allocator(), 1);
                parser->declaration_stack.reserve(cz::heap_allocator(), 1);
                parser->type_stack.push({});
                parser->typedef_stack.push({});
                parser->declaration_stack.push({});
                CZ_DEFER({
                    parser->type_stack.pop();
                    parser->typedef_stack.pop();
                    parser->declaration_stack.pop();
                });

                cz::Vector<Statement*> initializers = {};
                CZ_DEFER(initializers.drop(cz::heap_allocator()));
                CZ_TRY(parse_composite_body(context, parser, &initializers, &flags, struct_span));

                if (!struct_type) {
                    struct_type = parser->buffer_array.allocator().create<Type_Struct>();
                    if (identifier.str.len > 0) {
                        cz::Str_Map<Type*>* types =
                            &parser->type_stack[parser->type_stack.len() - 2];
                        types->reserve(cz::heap_allocator(), 1);
                        types->insert(identifier.str, identifier.hash, struct_type);
                    }
                }

                struct_type->types = parser->type_stack.last();
                struct_type->typedefs = parser->typedef_stack.last();
                struct_type->declarations = parser->declaration_stack.last();
                struct_type->initializers =
                    parser->buffer_array.allocator().duplicate(initializers.as_slice());
                struct_type->flags = flags;

                base_type->set_type(struct_type);
                break;
            } else {
                parser->back = token;
                Type** type = lookup_type(parser, identifier);
                if (type) {
                    if ((*type)->tag != Type::Struct) {
                        context->report_error(identifier_span, "Type `", identifier.str,
                                              "` is not a struct");
                    }
                    base_type->set_type(*type);
                } else {
                    Type_Struct* struct_type =
                        parser->buffer_array.allocator().create<Type_Struct>();
                    struct_type->types = {};
                    struct_type->typedefs = {};
                    struct_type->declarations = {};
                    struct_type->initializers = {};
                    struct_type->flags = 0;
                    cz::Str_Map<Type*>* types = &parser->type_stack.last();
                    types->reserve(cz::heap_allocator(), 1);
                    types->insert(identifier.str, identifier.hash, struct_type);
                    base_type->set_type(struct_type);
                }
                break;
            }
        }

        case Token::Union: {
            Span union_span = token.span;
            result = next_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(union_span, "Expected union name, body, or `;` here");
                return {Result::ErrorInvalidInput};
            }

            Span identifier_span;
            Hashed_Str identifier = {};
            if (token.type == Token::Identifier) {
                identifier = token.v.identifier;
                identifier_span = token.span;

                result = next_token(context, parser, &token);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(union_span, "Expected declaration, union body, `;` here");
                    return {Result::ErrorInvalidInput};
                }
            }

            if (token.type == Token::Semicolon) {
                if (identifier.str.len > 0) {
                    Type** type = lookup_type(parser, identifier);
                    if (type) {
                        if ((*type)->tag != Type::Union) {
                            context->report_error(identifier_span, "Type `", identifier.str,
                                                  "` is not a union");
                            return {Result::ErrorInvalidInput};
                        }
                    } else {
                        Type_Union* union_type =
                            parser->buffer_array.allocator().create<Type_Union>();
                        union_type->types = {};
                        union_type->typedefs = {};
                        union_type->declarations = {};
                        union_type->flags = 0;
                        cz::Str_Map<Type*>* types = &parser->type_stack.last();
                        types->reserve(cz::heap_allocator(), 1);
                        types->insert(identifier.str, identifier.hash, union_type);
                    }
                }
                parser->back = token;
                return Result::ok();
            }

            if (token.type == Token::OpenCurly) {
                Type** type = lookup_type(parser, identifier);
                Type_Union* union_type = nullptr;
                if (type) {
                    if ((*type)->tag != Type::Union) {
                        context->report_error(identifier_span, "Type `", identifier.str,
                                              "` is not a union");
                        return {Result::ErrorInvalidInput};
                    }
                    union_type = (Type_Union*)*type;
                    if (union_type->flags & Composite_Flags::Defined) {
                        context->report_error(identifier_span, "Type `", identifier.str,
                                              "` is already defined");
                        return {Result::ErrorInvalidInput};
                    }
                }

                uint32_t flags = Composite_Flags::Defined;

                parser->type_stack.reserve(cz::heap_allocator(), 1);
                parser->typedef_stack.reserve(cz::heap_allocator(), 1);
                parser->declaration_stack.reserve(cz::heap_allocator(), 1);
                parser->type_stack.push({});
                parser->typedef_stack.push({});
                parser->declaration_stack.push({});
                CZ_DEFER({
                    parser->type_stack.pop();
                    parser->typedef_stack.pop();
                    parser->declaration_stack.pop();
                });

                cz::Vector<Statement*> initializers = {};
                CZ_DEFER(initializers.drop(cz::heap_allocator()));
                CZ_TRY(parse_composite_body(context, parser, &initializers, &flags, union_span));

                for (size_t i = 0; i < initializers.len(); ++i) {
                    if (initializers[i]->tag != Statement::Initializer_Default) {
                        context->report_error(union_span,
                                              "Union variants cannot have initializers");
                    }
                }

                if (!union_type) {
                    union_type = parser->buffer_array.allocator().create<Type_Union>();
                    if (identifier.str.len > 0) {
                        cz::Str_Map<Type*>* types =
                            &parser->type_stack[parser->type_stack.len() - 2];
                        types->reserve(cz::heap_allocator(), 1);
                        types->insert(identifier.str, identifier.hash, union_type);
                    }
                }

                union_type->types = parser->type_stack.last();
                union_type->typedefs = parser->typedef_stack.last();
                union_type->declarations = parser->declaration_stack.last();
                union_type->flags = flags;

                base_type->set_type(union_type);
                break;
            } else {
                parser->back = token;
                Type** type = lookup_type(parser, identifier);
                if (type) {
                    if ((*type)->tag != Type::Union) {
                        context->report_error(identifier_span, "Type `", identifier.str,
                                              "` is not a union");
                    }
                    base_type->set_type(*type);
                } else {
                    Type_Union* union_type = parser->buffer_array.allocator().create<Type_Union>();
                    union_type->types = {};
                    union_type->typedefs = {};
                    union_type->declarations = {};
                    union_type->flags = 0;
                    cz::Str_Map<Type*>* types = &parser->type_stack.last();
                    types->reserve(cz::heap_allocator(), 1);
                    types->insert(identifier.str, identifier.hash, union_type);
                    base_type->set_type(union_type);
                }
                break;
            }
        }

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
                Type** tagged_type = lookup_type(parser, token.v.identifier);
                if (tagged_type) {
                    base_type->set_type(*tagged_type);
                    break;
                } else {
                    return {Result::ErrorInvalidInput};
                }
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

Result parse_declaration(Context* context, Parser* parser, cz::Vector<Statement*>* initializers) {
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
        parser->back.type = Token::Parser_Null_Token;
        return Result::ok();
    }

    while (1) {
        result =
            parse_declaration_after_base_type(context, parser, base_type, &token, initializers);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            break;
        }
    }

    return Result::ok();
}

Result parse_declaration_or_statement(Context* context,
                                      Parser* parser,
                                      cz::Vector<Statement*>* statements,
                                      Declaration_Or_Statement* which) {
    Token token;
    Result result = peek_token(context, parser, &token);
    if (result.type != Result::Success) {
        return result;
    }

    switch (token.type) {
        case Token::Char:
        case Token::Double:
        case Token::Float:
        case Token::Int:
        case Token::Long:
        case Token::Short:
        case Token::Void:
        case Token::Struct:
        case Token::Union:
        case Token::Const:
        case Token::Volatile: {
            *which = Declaration_Or_Statement::Declaration;
            return parse_declaration(context, parser, statements);
        }

        case Token::Identifier: {
            Declaration* declaration = lookup_declaration(parser, token.v.identifier);
            TypeP* type = lookup_typedef(parser, token.v.identifier);
            if (declaration) {
                *which = Declaration_Or_Statement::Statement;

                Statement* statement;
                result = parse_statement(context, parser, &statement);
                if (result.is_ok()) {
                    statements->reserve(cz::heap_allocator(), 1);
                    statements->push(statement);
                }
                return result;
            } else if (type) {
                *which = Declaration_Or_Statement::Declaration;
                return parse_declaration(context, parser, statements);
            } else {
                context->report_error(token.span, "Undefined identifier `", token.v.identifier.str,
                                      "`");
                return {Result::ErrorInvalidInput};
            }
        }

        default: {
            *which = Declaration_Or_Statement::Statement;

            Statement* statement;
            result = parse_statement(context, parser, &statement);
            if (result.is_ok()) {
                statements->reserve(cz::heap_allocator(), 1);
                statements->push(statement);
            }
            return result;
        }
    }
}

static Result parse_expression_(Context* context,
                                Parser* parser,
                                Expression** eout,
                                int max_precedence) {
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
            result = next_token(context, parser, &token);
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

Result parse_statement(Context* context, Parser* parser, Statement** sout) {
    Token token;
    Result result = peek_token(context, parser, &token);
    if (result.type != Result::Success) {
        return result;
    }

    switch (token.type) {
        case Token::OpenCurly: {
            Span open_curly_span = token.span;
            parser->back.type = Token::Parser_Null_Token;

            parser->type_stack.reserve(cz::heap_allocator(), 1);
            parser->typedef_stack.reserve(cz::heap_allocator(), 1);
            parser->declaration_stack.reserve(cz::heap_allocator(), 1);
            parser->type_stack.push({});
            parser->typedef_stack.push({});
            parser->declaration_stack.push({});
            CZ_DEFER({
                parser->type_stack.pop();
                parser->typedef_stack.pop();
                parser->declaration_stack.pop();
            });

            cz::Vector<Statement*> statements = {};
            CZ_DEFER(statements.drop(cz::heap_allocator()));
            while (1) {
                result = peek_token(context, parser, &token);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(
                        open_curly_span,
                        "Expected end of block to match start of block (`{`) here");
                    return {Result::ErrorInvalidInput};
                }
                if (token.type == Token::CloseCurly) {
                    parser->back.type = Token::Parser_Null_Token;
                    goto finish_block;
                }

                Declaration_Or_Statement which;
                CZ_TRY(parse_declaration_or_statement(context, parser, &statements, &which));

                if (which == Declaration_Or_Statement::Statement) {
                    break;
                }
            }

            while (1) {
                result = peek_token(context, parser, &token);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(
                        open_curly_span,
                        "Expected end of block to match start of block (`{`) here");
                    return {Result::ErrorInvalidInput};
                }
                if (token.type == Token::CloseCurly) {
                    parser->back.type = Token::Parser_Null_Token;
                    goto finish_block;
                }

                Statement* statement;
                CZ_TRY(parse_statement(context, parser, &statement));

                statements.reserve(cz::heap_allocator(), 1);
                statements.push(statement);
            }

        finish_block:
            Statement_Block* statement = parser->buffer_array.allocator().create<Statement_Block>();
            statement->statements =
                parser->buffer_array.allocator().duplicate(statements.as_slice());
            *sout = statement;
            return Result::ok();
        }

        case Token::While: {
            Span while_span = token.span;
            parser->back.type = Token::Parser_Null_Token;

            result = next_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(while_span, "Expected open parenthesis here");
                return {Result::ErrorInvalidInput};
            }
            if (token.type != Token::OpenParen) {
                context->report_error(token.span, "Expected open parenthesis here");
                return {Result::ErrorInvalidInput};
            }

            Expression* condition;
            result = parse_expression(context, parser, &condition);
            if (result.type == Result::Done) {
                context->report_error(while_span, "Expected condition expression here");
                return {Result::ErrorInvalidInput};
            }

            result = next_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(while_span, "Expected `)` to end condition expression");
                return {Result::ErrorInvalidInput};
            }
            if (token.type != Token::CloseParen) {
                context->report_error(token.span, "Expected `)` here to end condition expression");
                return {Result::ErrorInvalidInput};
            }

            Statement* body;
            result = parse_statement(context, parser, &body);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(while_span, "Expected body statement");
                return {Result::ErrorInvalidInput};
            }

            Statement_While* statement = parser->buffer_array.allocator().create<Statement_While>();
            statement->condition = condition;
            statement->body = body;
            *sout = statement;
            return Result::ok();
        }

        case Token::For: {
            Span for_span = token.span;
            parser->back.type = Token::Parser_Null_Token;

            result = next_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_span, "Expected open parenthesis here");
                return {Result::ErrorInvalidInput};
            }
            if (token.type != Token::OpenParen) {
                context->report_error(token.span, "Expected open parenthesis here");
                return {Result::ErrorInvalidInput};
            }

            result = peek_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_span, "Expected initializer or `;`");
                return {Result::ErrorInvalidInput};
            }
            Expression* initializer = nullptr;
            if (token.type == Token::Semicolon) {
                parser->back.type = Token::Parser_Null_Token;
            } else {
                CZ_TRY(parse_expression(context, parser, &initializer));

                result = next_token(context, parser, &token);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(for_span, "Expected `;` to end initializer expression");
                    return {Result::ErrorInvalidInput};
                }
                if (token.type != Token::Semicolon) {
                    context->report_error(token.span,
                                          "Expected `;` here to end initializer expression");
                    return {Result::ErrorInvalidInput};
                }
            }

            result = peek_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_span, "Expected condition expression or `;` here");
                return {Result::ErrorInvalidInput};
            }
            Expression* condition = nullptr;
            if (token.type == Token::Semicolon) {
                parser->back.type = Token::Parser_Null_Token;
            } else {
                CZ_TRY(parse_expression(context, parser, &condition));

                result = next_token(context, parser, &token);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(for_span, "Expected `;` to end condition expression");
                    return {Result::ErrorInvalidInput};
                }
                if (token.type != Token::Semicolon) {
                    context->report_error(token.span,
                                          "Expected `;` here to end condition expression");
                    return {Result::ErrorInvalidInput};
                }
            }

            result = peek_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_span, "Expected increment expression or `)`");
                return {Result::ErrorInvalidInput};
            }
            Expression* increment = nullptr;
            if (token.type == Token::CloseParen) {
                parser->back.type = Token::Parser_Null_Token;
            } else {
                CZ_TRY(parse_expression(context, parser, &increment));

                result = next_token(context, parser, &token);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(for_span, "Expected `)` to end increment expression");
                    return {Result::ErrorInvalidInput};
                }
                if (token.type != Token::CloseParen) {
                    context->report_error(token.span,
                                          "Expected `)` here to end increment expression");
                    return {Result::ErrorInvalidInput};
                }
            }

            Statement* body;
            result = parse_statement(context, parser, &body);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_span, "Expected body statement");
                return {Result::ErrorInvalidInput};
            }

            Statement_For* statement = parser->buffer_array.allocator().create<Statement_For>();
            statement->initializer = initializer;
            statement->condition = condition;
            statement->increment = increment;
            statement->body = body;
            *sout = statement;
            return Result::ok();
        }

        default: {
            Expression* expression;
            CZ_TRY(parse_expression(context, parser, &expression));

            Span span = token.span;
            result = next_token(context, parser, &token);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(span, "Expected semicolon here to end expression statement");
                return {Result::ErrorInvalidInput};
            }

            Statement_Expression* statement =
                parser->buffer_array.allocator().create<Statement_Expression>();
            statement->expression = expression;
            *sout = statement;
            return Result::ok();
        }
    }
}

}
}
