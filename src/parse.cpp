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
    type_signed_char = make_primitive(buffer_array.allocator(), Type::Builtin_Signed_Char);
    type_unsigned_char = make_primitive(buffer_array.allocator(), Type::Builtin_Unsigned_Char);

    type_float = make_primitive(buffer_array.allocator(), Type::Builtin_Float);
    type_double = make_primitive(buffer_array.allocator(), Type::Builtin_Double);
    type_long_double = make_primitive(buffer_array.allocator(), Type::Builtin_Long_Double);

    type_signed_short = make_primitive(buffer_array.allocator(), Type::Builtin_Signed_Short);
    type_signed_int = make_primitive(buffer_array.allocator(), Type::Builtin_Signed_Int);
    type_signed_long = make_primitive(buffer_array.allocator(), Type::Builtin_Signed_Long);
    type_signed_long_long =
        make_primitive(buffer_array.allocator(), Type::Builtin_Signed_Long_Long);
    type_unsigned_short = make_primitive(buffer_array.allocator(), Type::Builtin_Unsigned_Short);
    type_unsigned_int = make_primitive(buffer_array.allocator(), Type::Builtin_Unsigned_Int);
    type_unsigned_long = make_primitive(buffer_array.allocator(), Type::Builtin_Unsigned_Long);
    type_unsigned_long_long =
        make_primitive(buffer_array.allocator(), Type::Builtin_Unsigned_Long_Long);

    type_void = make_primitive(buffer_array.allocator(), Type::Builtin_Void);
    type_error = make_primitive(buffer_array.allocator(), Type::Builtin_Error);

    pairs[0].token.type = Token::Parser_Null_Token;
    pairs[1].token.type = Token::Parser_Null_Token;

    type_stack.reserve(cz::heap_allocator(), 4);
    typedef_stack.reserve(cz::heap_allocator(), 4);
    declaration_stack.reserve(cz::heap_allocator(), 4);
    type_stack.push({});
    typedef_stack.push({});
    declaration_stack.push({});
}

void Parser::drop() {
    for (size_t i = 0; i < type_stack.len(); ++i) {
        drop_types(&type_stack[i]);
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

void drop_type(Type* type) {
    switch (type->tag) {
        case Type::Enum: {
            Type_Enum* t = (Type_Enum*)type;
            t->values.drop(cz::heap_allocator());
            break;
        }

        case Type::Struct:
        case Type::Union: {
            Type_Composite* t = (Type_Composite*)type;
            drop_types(&t->types);
            t->typedefs.drop(cz::heap_allocator());
            t->declarations.drop(cz::heap_allocator());
            break;
        }

        default:
            break;
    }
}

void drop_types(cz::Str_Map<Type*>* types) {
    for (size_t i = 0; i < types->count; ++i) {
        if (types->is_present(i)) {
            drop_type(types->values[i]);
        }
    }

    types->drop(cz::heap_allocator());
}

static Result peek_token(Context* context, Parser* parser, Token_Source_Span_Pair* pair_out) {
    Token_Source_Span_Pair* pair = &parser->pairs[parser->pair_index];
    if (pair->token.type == Token::Parser_Null_Token) {
        Result result =
            cpp::next_token(context, &parser->preprocessor, &parser->lexer, &pair->token);
        if (result.type == Result::Success) {
            Location location = parser->preprocessor.location();
            pair->source_span = {location, location};
            *pair_out = *pair;
        } else {
            pair->token.type = Token::Parser_Null_Token;
        }
        return result;
    } else {
        *pair_out = *pair;
        return Result::ok();
    }
}

static void next_token_after_peek(Parser* parser) {
    parser->pair_index = 1 - parser->pair_index;
    parser->pairs[parser->pair_index].token.type = Token::Parser_Null_Token;
}

static Result next_token(Context* context, Parser* parser, Token_Source_Span_Pair* pair) {
    Result result = peek_token(context, parser, pair);
    if (result.type == Result::Success) {
        next_token_after_peek(parser);
    }
    return result;
}

static void reverse_next_token(Parser* parser) {
    parser->pair_index = 1 - parser->pair_index;
}

static void previous_token(Parser* parser, Token_Source_Span_Pair* pair) {
    *pair = parser->pairs[1 - parser->pair_index];
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

static void parse_const(Context* context, TypeP* type, Token_Source_Span_Pair pair) {
    if (type->is_const()) {
        context->report_error(pair.token.span, pair.source_span, "Multiple `const` attributes");
    }
    type->set_const();
}

static void parse_volatile(Context* context, TypeP* type, Token_Source_Span_Pair pair) {
    if (type->is_volatile()) {
        context->report_error(pair.token.span, pair.source_span, "Multiple `volatile` attributes");
    }
    type->set_volatile();
}

static Result parse_expression_(Context* context,
                                Parser* parser,
                                Expression** eout,
                                int max_precedence);

static Result parse_base_type(Context* context,
                              Parser* parser,
                              cz::Vector<Statement*>* initializers,
                              TypeP* base_type);

static Result parse_declaration_identifier_and_type(Context* context,
                                                    Parser* parser,
                                                    cz::Vector<Statement*>* initializers,
                                                    Hashed_Str* identifier,
                                                    TypeP* type,
                                                    TypeP** inner_type_out,
                                                    cz::Slice<cz::Str>* parameter_names);

static Result parse_parameters(Context* context,
                               Parser* parser,
                               cz::Vector<Statement*>* initializers,
                               cz::Vector<TypeP>* parameter_types,
                               cz::Vector<cz::Str>* parameter_names,
                               bool* has_varargs) {
    Token_Source_Span_Pair pair;
    next_token(context, parser, &pair);
    Span previous_span = pair.token.span;
    Span previous_source_span = pair.source_span;
    Result result = peek_token(context, parser, &pair);
    CZ_TRY_VAR(result);
    if (result.type == Result::Done) {
        context->report_error(previous_span, previous_source_span,
                              "Expected ')' to end parameter list here");
        return {Result::ErrorInvalidInput};
    }
    if (pair.token.type == Token::CloseParen) {
        next_token_after_peek(parser);
        return Result::ok();
    }

    while (1) {
        TypeP type = {};
        result = parse_base_type(context, parser, initializers, &type);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(previous_span, previous_source_span,
                                  "Expected ')' to end parameter list here");
            return {Result::ErrorInvalidInput};
        }

        previous_span = pair.token.span;
        previous_source_span = pair.source_span;
        result = peek_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(previous_span, previous_source_span,
                                  "Expected ')' to end parameter list here");
            return {Result::ErrorInvalidInput};
        }

        Hashed_Str identifier = {};
        if (pair.token.type != Token::CloseParen && pair.token.type != Token::Comma) {
            cz::Slice<cz::Str> inner_parameter_names;
            CZ_TRY(parse_declaration_identifier_and_type(context, parser, initializers, &identifier,
                                                         &type, nullptr, &inner_parameter_names));
        }

        parameter_types->reserve(cz::heap_allocator(), 1);
        parameter_names->reserve(cz::heap_allocator(), 1);
        parameter_types->push(type);
        parameter_names->push(identifier.str);

        previous_span = pair.token.span;
        previous_source_span = pair.source_span;
        result = next_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(previous_span, previous_source_span,
                                  "Expected ')' to end parameter list here");
            return {Result::ErrorInvalidInput};
        }

        if (pair.token.type == Token::CloseParen) {
            return Result::ok();
        } else if (pair.token.type == Token::Comma) {
            continue;
        } else {
            context->report_error(pair.token.span, pair.source_span,
                                  "Expected ')' to end parameter list here");
            return {Result::ErrorInvalidInput};
        }
    }
}

static Result parse_block(Context* context, Parser* parser, Block* block);

static Result parse_declaration_initializer(Context* context,
                                            Parser* parser,
                                            Declaration declaration,
                                            Hashed_Str identifier,
                                            cz::Vector<Statement*>* initializers,
                                            cz::Slice<cz::Str> parameter_names,
                                            bool* force_terminate) {
    // Todo: support arrays
    Token_Source_Span_Pair pair;
    previous_token(parser, &pair);
    Span previous_span = pair.token.span;
    Span previous_source_span = pair.source_span;
    Result result = peek_token(context, parser, &pair);

    if (declaration.type.get_type()->tag == Type::Function) {
        switch (pair.token.type) {
            case Token::Set: {
                context->report_error(pair.token.span, pair.source_span,
                                      "Function must be defined with `{` not assigned with `=`");
                return {Result::ErrorInvalidInput};
            }

            case Token::OpenCurly: {
                cz::Str_Map<Declaration> parameters = {};
                parameters.reserve(cz::heap_allocator(), parameter_names.len);
                Type_Function* fun = (Type_Function*)declaration.type.get_type();
                for (size_t i = 0; i < parameter_names.len; ++i) {
                    Declaration declaration = {};
                    declaration.type = fun->parameter_types[i];
                    parameters.insert_hash(parameter_names[i], declaration);
                }

                parser->type_stack.reserve(cz::heap_allocator(), 1);
                parser->typedef_stack.reserve(cz::heap_allocator(), 1);
                parser->declaration_stack.reserve(cz::heap_allocator(), 1);
                parser->type_stack.push({});
                parser->typedef_stack.push({});
                parser->declaration_stack.push(parameters);
                CZ_DEFER({
                    drop_types(&parser->type_stack.last());
                    parser->typedef_stack.last().drop(cz::heap_allocator());
                    parser->declaration_stack.last().drop(cz::heap_allocator());
                    parser->type_stack.pop();
                    parser->typedef_stack.pop();
                    parser->declaration_stack.pop();
                });

                Block block;
                CZ_TRY(parse_block(context, parser, &block));

                Function_Definition* function_definition =
                    parser->buffer_array.allocator().create<Function_Definition>();
                function_definition->parameter_names = parameter_names;
                function_definition->block = block;
                declaration.v.function_definition = function_definition;

                *force_terminate = true;
            } break;

            default:
                break;
        }
    } else {
        switch (pair.token.type) {
            case Token::Set: {
                next_token_after_peek(parser);

                // Eat the value.
                Expression* value;
                result = parse_expression_(context, parser, &value, 17);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(previous_span, previous_source_span,
                                          "Expected ';' to end declaration here");
                    return {Result::ErrorInvalidInput};
                }

                Statement_Initializer_Copy* initializer =
                    parser->buffer_array.allocator().create<Statement_Initializer_Copy>();
                initializer->identifier = identifier;
                initializer->value = value;
                initializers->reserve(cz::heap_allocator(), 1);
                initializers->push(initializer);
                declaration.v.initializer = initializer;
            } break;

            default: {
                Statement_Initializer_Default* initializer =
                    parser->buffer_array.allocator().create<Statement_Initializer_Default>();
                initializer->identifier = identifier;
                initializers->reserve(cz::heap_allocator(), 1);
                initializers->push(initializer);
                declaration.v.initializer = initializer;
            } break;
        }
    }

    cz::Str_Map<Declaration>* declarations = &parser->declaration_stack.last();
    if (!declarations->get(identifier.str, identifier.hash)) {
        declarations->reserve(cz::heap_allocator(), 1);
        declarations->insert(identifier.str, identifier.hash, declaration);
    } else {
        context->report_error(previous_span, previous_source_span,
                              "Declaration with same name also in scope");
    }
    return Result::ok();
}

static Result parse_declaration_identifier_and_type(Context* context,
                                                    Parser* parser,
                                                    cz::Vector<Statement*>* initializers,
                                                    Hashed_Str* identifier,
                                                    TypeP* overall_type,
                                                    TypeP** inner_type_out,
                                                    cz::Slice<cz::Str>* parameter_names_out) {
    /// We need to parse `(*function)(params)` to `Pointer(Function([params], base type))`.
    /// To do this we need to have the pointer (`*function`) apply after the function (`(params)`).
    /// We do this by storing pointers to inside the data structures and fixing them later.
    ///
    /// `inner_type_out` allows an outer call to have a pointer to `x` in `Pointer(x)`.
    /// This is assigned to on the first wrap and then made null (so consecutive wraps don't assign
    /// to it).
    ///
    /// `type` stores the current node that should be wrapped.  That is, if we hit a function
    /// wrapper, we should wrap the `type`.
    ///
    /// # Example
    ///
    /// In `int (*abc)()`, The overall type is `int` and `inner_type_out` is `nullptr` since we are
    /// in outermost call.  We first see `(` and recurse.
    ///
    /// In the recursive call, `type` is `int` and `*inner_type_out` is `type` in the outer call. We
    /// see `*` and wrap `type` with a pointer.  Our overall type is `Pointer(int)`.  We set
    /// `*inner_type_out` (outer `type`) to point to the inner `int`.  We return and then we eat the
    /// `)`.
    ///
    /// Now we see `(`.  The overall type is `Pointer(int)`, but `type` points to the inner `int`.
    /// We parse the parameters and then wrap a function type around `type`.  This results in
    /// `Pointer(Function([], int))`
    TypeP* type = overall_type;
    bool allow_qualifiers = false;
    bool already_hit_identifier = false;
    Token_Source_Span_Pair pair;
    previous_token(parser, &pair);
    while (1) {
        Span previous_span = pair.token.span;
        Span previous_source_span = pair.source_span;
        Result result = peek_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            return Result::ok();
        }

        switch (pair.token.type) {
            case Token::Star: {
                if (already_hit_identifier || identifier->str.len > 0) {
                    return Result::ok();
                }

                next_token_after_peek(parser);

                Type_Pointer* pointer = parser->buffer_array.allocator().create<Type_Pointer>();
                pointer->inner = *type;
                if (inner_type_out) {
                    *inner_type_out = &pointer->inner;
                    inner_type_out = nullptr;
                }
                type->clear();
                type->set_type(pointer);
                allow_qualifiers = true;
            } break;

            case Token::OpenParen: {
                if (already_hit_identifier || identifier->str.len > 0) {
                    cz::Vector<TypeP> parameter_types = {};
                    cz::Vector<cz::Str> parameter_names = {};
                    bool has_varargs = false;
                    CZ_DEFER({
                        parameter_types.drop(cz::heap_allocator());
                        parameter_names.drop(cz::heap_allocator());
                    });

                    CZ_TRY(parse_parameters(context, parser, initializers, &parameter_types,
                                            &parameter_names, &has_varargs));
                    if (type == overall_type) {
                        *parameter_names_out =
                            parser->buffer_array.allocator().duplicate(parameter_names.as_slice());
                    }

                    Type_Function* function =
                        parser->buffer_array.allocator().create<Type_Function>();
                    function->return_type = *type;
                    function->parameter_types =
                        parser->buffer_array.allocator().duplicate(parameter_types.as_slice());
                    function->has_varargs = has_varargs;
                    if (inner_type_out) {
                        *inner_type_out = &function->return_type;
                        inner_type_out = nullptr;
                    }
                    type->clear();
                    type->set_type(function);
                    return Result::ok();
                } else {
                    previous_span = pair.token.span;
                    previous_source_span = pair.source_span;

                    next_token_after_peek(parser);

                    CZ_TRY(parse_declaration_identifier_and_type(context, parser, initializers,
                                                                 identifier, type, &type,
                                                                 parameter_names_out));
                    already_hit_identifier = true;

                    result = next_token(context, parser, &pair);
                    CZ_TRY_VAR(result);
                    if (result.type == Result::Done || pair.token.type != Token::CloseParen) {
                        context->report_error(previous_span, previous_source_span,
                                              "Expected ')' to match '(' here");
                        return {Result::ErrorInvalidInput};
                    }
                }
            } break;

            case Token::OpenSquare: {
                previous_span = pair.token.span;
                previous_source_span = pair.source_span;

                next_token_after_peek(parser);

                result = peek_token(context, parser, &pair);
                CZ_TRY_VAR(result);

                Expression* expression;
                if (result.type == Result::Success && pair.token.type == Token::CloseSquare) {
                    expression = nullptr;
                } else {
                    result = parse_expression(context, parser, &expression);
                    CZ_TRY_VAR(result);
                    if (result.type == Result::Done) {
                        context->report_error(previous_span, previous_source_span,
                                              "Expected size expression or `]` here");
                        break;
                    }

                    result = next_token(context, parser, &pair);
                    CZ_TRY_VAR(result);
                    if (result.type == Result::Done) {
                        context->report_error(previous_span, previous_source_span,
                                              "Expected `]` to match `[` here");
                        return {Result::ErrorInvalidInput};
                    }
                    if (pair.token.type != Token::CloseSquare) {
                        context->report_error(pair.token.span, pair.source_span,
                                              "Expected `]` here");
                        context->report_error(previous_span, previous_source_span,
                                              "Expected `]` to match `[` here");
                        return {Result::ErrorInvalidInput};
                    }
                }

                Type_Array* array = parser->buffer_array.allocator().create<Type_Array>();
                array->inner = *type;
                array->size = expression;
                if (inner_type_out) {
                    *inner_type_out = &array->inner;
                    inner_type_out = nullptr;
                }
                type->clear();
                type->set_type(array);
            } break;

            case Token::Const:
                if (already_hit_identifier || identifier->str.len > 0) {
                    return Result::ok();
                }

                next_token_after_peek(parser);

                if (!allow_qualifiers) {
                    context->report_error(pair.token.span, pair.source_span,
                                          "East const must be used immediately after base type");
                    break;
                }
                parse_const(context, type, pair);
                break;

            case Token::Volatile:
                if (already_hit_identifier || identifier->str.len > 0) {
                    return Result::ok();
                }

                next_token_after_peek(parser);

                if (!allow_qualifiers) {
                    context->report_error(pair.token.span, pair.source_span,
                                          "East volatile must be used immediately after base type");
                    break;
                }
                parse_volatile(context, type, pair);
                break;

            case Token::Identifier:
                if (identifier->str.len > 0) {
                    return Result::ok();
                }

                next_token_after_peek(parser);

                *identifier = pair.token.v.identifier;
                break;

            default:
                return Result::ok();
        }
    }
}

static Result parse_composite_body(Context* context,
                                   Parser* parser,
                                   cz::Vector<Statement*>* initializers,
                                   uint32_t* flags,
                                   Span composite_span,
                                   Span composite_source_span) {
    while (1) {
        Token_Source_Span_Pair pair;
        Result result = peek_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(composite_span, composite_source_span,
                                  "Expected close curly (`}`) to end body");
            return {Result::ErrorInvalidInput};
        }
        if (pair.token.type == Token::CloseCurly) {
            next_token_after_peek(parser);
            return Result::ok();
        }

        CZ_TRY(parse_declaration(context, parser, initializers));
    }
}

static Result parse_enum_body(Context* context,
                              Parser* parser,
                              cz::Str_Map<int64_t>* values,
                              uint32_t* flags,
                              Span enum_span,
                              Span enum_source_span) {
    for (int64_t value = 0;; ++value) {
        Token_Source_Span_Pair pair;
        Result result = next_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(enum_span, enum_source_span,
                                  "Expected close curly (`}`) to end enum body");
            return {Result::ErrorInvalidInput};
        }
        if (pair.token.type == Token::CloseCurly) {
            return Result::ok();
        }

        if (pair.token.type != Token::Identifier) {
            context->report_error(pair.token.span, pair.source_span,
                                  "Expected identifier for enum member");
            return {Result::ErrorInvalidInput};
        }

        Hashed_Str name = pair.token.v.identifier;
        Token_Source_Span_Pair name_pair = pair;

        result = next_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(enum_span, enum_source_span,
                                  "Expected close curly (`}`) to end enum body");
            return {Result::ErrorInvalidInput};
        }
        if (pair.token.type == Token::Set) {
            result = next_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(enum_span, enum_source_span,
                                      "Expected close curly (`}`) to end enum body");
                return {Result::ErrorInvalidInput};
            }

            if (pair.token.type != Token::Integer) {
                CZ_PANIC("Unimplemented expression enum value");
            }

            value = pair.token.v.integer.value;

            result = next_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(enum_span, enum_source_span,
                                      "Expected close curly (`}`) to end enum body");
                return {Result::ErrorInvalidInput};
            }
        }

        values->reserve(cz::heap_allocator(), 1);
        if (!values->get(name.str, name.hash)) {
            values->insert(name.str, name.hash, value);
        } else {
            context->report_error(name_pair.token.span, name_pair.source_span,
                                  "Enum member is already defined");
        }

        if (pair.token.type == Token::CloseCurly) {
            return Result::ok();
        }
        if (pair.token.type != Token::Comma) {
            context->report_error(
                enum_span, enum_source_span,
                "Expected `,` to continue enum body or close curly (`}`) to end enum body");
            return {Result::ErrorInvalidInput};
        }
    }
}

struct Numeric_Base {
    uint32_t flags;

    enum : uint32_t {
        Char_Index = 1 - 1,
        Double_Index = 2 - 1,
        Float_Index = 3 - 1,
        Int_Index = 4 - 1,
        Short_Index = 5 - 1,
        Long_Index = 6 - 1,
        Long_Long_Index = 7 - 1,
        Signed_Index = 8 - 1,
        Unsigned_Index = 9 - 1,

        Char = 1 << Char_Index + 1,
        Double = 1 << Double_Index + 1,
        Float = 1 << Float_Index + 1,
        Int = 1 << Int_Index + 1,
        Short = 1 << Short_Index + 1,
        Long = 1 << Long_Index + 1,
        Long_Long = 1 << Long_Long_Index + 1,
        Signed = 1 << Signed_Index + 1,
        Unsigned = 1 << Unsigned_Index + 1,
    };
};

static Result parse_base_type(Context* context,
                              Parser* parser,
                              cz::Vector<Statement*>* initializers,
                              TypeP* base_type) {
    // Todo: dealloc anonymous structures.  This will probably work by copying the type information
    // into the buffer array.

    Token_Source_Span_Pair pair;
    Result result;

    result = peek_token(context, parser, &pair);
    if (result.type != Result::Success) {
        return result;
    }

    Numeric_Base numeric_base = {};
    Span numeric_base_spans[9 * 2];

    while (1) {
        result = peek_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            break;
        }

        switch (pair.token.type) {
            case Token::Struct: {
                Span struct_span = pair.token.span;
                Span struct_source_span = pair.source_span;
                next_token_after_peek(parser);
                result = peek_token(context, parser, &pair);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(struct_span, struct_source_span,
                                          "Expected struct name, body, or `;` here");
                    return {Result::ErrorInvalidInput};
                }

                Span identifier_span;
                Span identifier_source_span;
                Hashed_Str identifier = {};
                if (pair.token.type == Token::Identifier) {
                    identifier = pair.token.v.identifier;
                    identifier_span = pair.token.span;
                    identifier_source_span = pair.source_span;

                    next_token_after_peek(parser);
                    result = peek_token(context, parser, &pair);
                    CZ_TRY_VAR(result);
                    if (result.type == Result::Done) {
                        context->report_error(struct_span, struct_source_span,
                                              "Expected declaration, struct body, `;` here");
                        return {Result::ErrorInvalidInput};
                    }
                }

                if (pair.token.type == Token::Semicolon) {
                    if (identifier.str.len > 0) {
                        Type** type = lookup_type(parser, identifier);
                        if (type) {
                            if ((*type)->tag != Type::Struct) {
                                context->report_error(identifier_span, identifier_source_span,
                                                      "Type `", identifier.str,
                                                      "` is not a struct");
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
                    return Result::ok();
                }

                if (pair.token.type == Token::OpenCurly) {
                    next_token_after_peek(parser);

                    Type** type = lookup_type(parser, identifier);
                    Type_Struct* struct_type = nullptr;
                    if (type) {
                        if ((*type)->tag == Type::Struct) {
                            struct_type = (Type_Struct*)*type;
                            if (struct_type->flags & Type_Struct::Defined) {
                                context->report_error(identifier_span, identifier_source_span,
                                                      "Type `", identifier.str,
                                                      "` is already defined");
                            } else {
                                type = nullptr;
                            }
                        } else {
                            context->report_error(identifier_span, identifier_source_span, "Type `",
                                                  identifier.str, "` is not a struct");
                        }
                    }

                    uint32_t flags = Type_Struct::Defined;

                    parser->type_stack.reserve(cz::heap_allocator(), 1);
                    parser->typedef_stack.reserve(cz::heap_allocator(), 1);
                    parser->declaration_stack.reserve(cz::heap_allocator(), 1);
                    parser->type_stack.push({});
                    parser->typedef_stack.push({});
                    parser->declaration_stack.push({});
                    bool cleanup_last_stack = true;
                    CZ_DEFER({
                        if (cleanup_last_stack) {
                            drop_types(&parser->type_stack.last());
                            parser->typedef_stack.last().drop(cz::heap_allocator());
                            parser->declaration_stack.last().drop(cz::heap_allocator());
                        }
                        parser->type_stack.pop();
                        parser->typedef_stack.pop();
                        parser->declaration_stack.pop();
                    });

                    cz::Vector<Statement*> initializers = {};
                    CZ_DEFER(initializers.drop(cz::heap_allocator()));
                    CZ_TRY(parse_composite_body(context, parser, &initializers, &flags, struct_span,
                                                struct_source_span));

                    // If type is already defined, just don't define it again.  This allows us to
                    // continue parsing which is good.
                    if (!type) {
                        if (!struct_type) {
                            struct_type = parser->buffer_array.allocator().create<Type_Struct>();
                            if (identifier.str.len > 0) {
                                cz::Str_Map<Type*>* types =
                                    &parser->type_stack[parser->type_stack.len() - 2];
                                types->reserve(cz::heap_allocator(), 1);
                                types->insert(identifier.str, identifier.hash, struct_type);
                            }
                        }

                        cleanup_last_stack = false;
                        struct_type->types = parser->type_stack.last();
                        struct_type->typedefs = parser->typedef_stack.last();
                        struct_type->declarations = parser->declaration_stack.last();
                        struct_type->initializers =
                            parser->buffer_array.allocator().duplicate(initializers.as_slice());
                        struct_type->flags = flags;

                        base_type->set_type(struct_type);
                    } else {
                        base_type->set_type(*type);
                    }
                    break;
                } else {
                    if (identifier.str.len > 0) {
                        Type** type = lookup_type(parser, identifier);
                        if (type) {
                            if ((*type)->tag != Type::Struct) {
                                context->report_error(identifier_span, identifier_source_span,
                                                      "Type `", identifier.str,
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
                    } else {
                        context->report_error(
                            struct_span, struct_source_span,
                            "Structs must be either named or anonymously defined");
                        base_type->set_type(parser->type_error);
                    }
                    break;
                }
            }

            case Token::Union: {
                Span union_span = pair.token.span;
                Span union_source_span = pair.source_span;
                next_token_after_peek(parser);
                result = peek_token(context, parser, &pair);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(union_span, union_source_span,
                                          "Expected union name, body, or `;` here");
                    return {Result::ErrorInvalidInput};
                }

                Span identifier_span;
                Span identifier_source_span;
                Hashed_Str identifier = {};
                if (pair.token.type == Token::Identifier) {
                    identifier = pair.token.v.identifier;
                    identifier_span = pair.token.span;
                    identifier_source_span = pair.source_span;

                    next_token_after_peek(parser);
                    result = peek_token(context, parser, &pair);
                    CZ_TRY_VAR(result);
                    if (result.type == Result::Done) {
                        context->report_error(union_span, union_source_span,
                                              "Expected declaration, union body, `;` here");
                        return {Result::ErrorInvalidInput};
                    }
                }

                if (pair.token.type == Token::Semicolon) {
                    if (identifier.str.len > 0) {
                        Type** type = lookup_type(parser, identifier);
                        if (type) {
                            if ((*type)->tag != Type::Union) {
                                context->report_error(identifier_span, identifier_source_span,
                                                      "Type `", identifier.str, "` is not a union");
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
                    return Result::ok();
                }

                if (pair.token.type == Token::OpenCurly) {
                    next_token_after_peek(parser);

                    Type** type = lookup_type(parser, identifier);
                    Type_Union* union_type = nullptr;
                    if (type) {
                        if ((*type)->tag == Type::Union) {
                            union_type = (Type_Union*)*type;
                            if (union_type->flags & Type_Union::Defined) {
                                context->report_error(identifier_span, identifier_source_span,
                                                      "Type `", identifier.str,
                                                      "` is already defined");
                            } else {
                                type = nullptr;
                            }
                        } else {
                            context->report_error(identifier_span, identifier_source_span, "Type `",
                                                  identifier.str, "` is not a union");
                        }
                    }

                    uint32_t flags = Type_Union::Defined;

                    parser->type_stack.reserve(cz::heap_allocator(), 1);
                    parser->typedef_stack.reserve(cz::heap_allocator(), 1);
                    parser->declaration_stack.reserve(cz::heap_allocator(), 1);
                    parser->type_stack.push({});
                    parser->typedef_stack.push({});
                    parser->declaration_stack.push({});
                    bool cleanup_last_stack = true;
                    CZ_DEFER({
                        if (cleanup_last_stack) {
                            drop_types(&parser->type_stack.last());
                            parser->typedef_stack.last().drop(cz::heap_allocator());
                            parser->declaration_stack.last().drop(cz::heap_allocator());
                        }
                        parser->type_stack.pop();
                        parser->typedef_stack.pop();
                        parser->declaration_stack.pop();
                    });

                    cz::Vector<Statement*> initializers = {};
                    CZ_DEFER(initializers.drop(cz::heap_allocator()));
                    CZ_TRY(parse_composite_body(context, parser, &initializers, &flags, union_span,
                                                union_source_span));

                    for (size_t i = 0; i < initializers.len(); ++i) {
                        if (initializers[i]->tag != Statement::Initializer_Default) {
                            context->report_error(union_span, union_source_span,
                                                  "Union variants cannot have initializers");
                        }
                    }

                    if (!type) {
                        if (!union_type) {
                            union_type = parser->buffer_array.allocator().create<Type_Union>();
                            if (identifier.str.len > 0) {
                                cz::Str_Map<Type*>* types =
                                    &parser->type_stack[parser->type_stack.len() - 2];
                                types->reserve(cz::heap_allocator(), 1);
                                types->insert(identifier.str, identifier.hash, union_type);
                            }
                        }

                        cleanup_last_stack = false;
                        union_type->types = parser->type_stack.last();
                        union_type->typedefs = parser->typedef_stack.last();
                        union_type->declarations = parser->declaration_stack.last();
                        union_type->flags = flags;

                        base_type->set_type(union_type);
                    } else {
                        base_type->set_type(*type);
                    }
                    break;
                } else {
                    if (identifier.str.len > 0) {
                        Type** type = lookup_type(parser, identifier);
                        if (type) {
                            if ((*type)->tag != Type::Union) {
                                context->report_error(identifier_span, identifier_source_span,
                                                      "Type `", identifier.str, "` is not a union");
                            }
                            base_type->set_type(*type);
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
                            base_type->set_type(union_type);
                        }
                    } else {
                        context->report_error(union_span, union_source_span,
                                              "Unions must be either named or anonymously defined");
                        base_type->set_type(parser->type_error);
                    }
                    break;
                }
            }

            case Token::Enum: {
                Span enum_span = pair.token.span;
                Span enum_source_span = pair.source_span;
                next_token_after_peek(parser);
                result = peek_token(context, parser, &pair);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(enum_span, enum_source_span,
                                          "Expected enum name, body, or `;` here");
                    return {Result::ErrorInvalidInput};
                }

                Span identifier_span;
                Span identifier_source_span;
                Hashed_Str identifier = {};
                if (pair.token.type == Token::Identifier) {
                    identifier = pair.token.v.identifier;
                    identifier_span = pair.token.span;
                    identifier_source_span = pair.source_span;

                    next_token_after_peek(parser);
                    result = peek_token(context, parser, &pair);
                    CZ_TRY_VAR(result);
                    if (result.type == Result::Done) {
                        context->report_error(enum_span, enum_source_span,
                                              "Expected declaration, enum body, `;` here");
                        return {Result::ErrorInvalidInput};
                    }
                }

                if (pair.token.type == Token::Semicolon) {
                    if (identifier.str.len > 0) {
                        Type** type = lookup_type(parser, identifier);
                        if (type) {
                            if ((*type)->tag != Type::Enum) {
                                context->report_error(identifier_span, identifier_source_span,
                                                      "Type `", identifier.str, "` is not an enum");
                            }
                        } else {
                            Type_Enum* enum_type =
                                parser->buffer_array.allocator().create<Type_Enum>();
                            enum_type->values = {};
                            enum_type->flags = 0;
                            cz::Str_Map<Type*>* types = &parser->type_stack.last();
                            types->reserve(cz::heap_allocator(), 1);
                            types->insert(identifier.str, identifier.hash, enum_type);
                        }
                    }
                    return Result::ok();
                }

                if (pair.token.type == Token::OpenCurly) {
                    next_token_after_peek(parser);

                    Type** type = lookup_type(parser, identifier);
                    Type_Enum* enum_type = nullptr;
                    if (type) {
                        if ((*type)->tag == Type::Enum) {
                            enum_type = (Type_Enum*)*type;
                            if (enum_type->flags & Type_Enum::Defined) {
                                context->report_error(identifier_span, identifier_source_span,
                                                      "Type `", identifier.str,
                                                      "` is already defined");
                            } else {
                                type = nullptr;
                            }
                        } else {
                            context->report_error(identifier_span, identifier_source_span, "Type `",
                                                  identifier.str, "` is not an enum");
                        }
                    }

                    uint32_t flags = Type_Enum::Defined;

                    cz::Str_Map<int64_t> values = {};
                    bool destroy_values = true;
                    CZ_DEFER(if (destroy_values) { values.drop(cz::heap_allocator()); });

                    CZ_TRY(parse_enum_body(context, parser, &values, &flags, enum_span,
                                           enum_source_span));

                    cz::Str_Map<Declaration>* declarations = &parser->declaration_stack.last();
                    declarations->reserve(cz::heap_allocator(), values.cap);
                    initializers->reserve(cz::heap_allocator(), values.cap);
                    for (size_t i = 0; i < values.cap; ++i) {
                        if (values.is_present(i)) {
                            Hashed_Str key = Hashed_Str::from_str(values.keys[i]);
                            if (!declarations->get(key.str, key.hash)) {
                                Declaration declaration = {};
                                // Todo: expand to long / long long when values are too big
                                declaration.type.set_type(parser->type_signed_int);
                                declaration.type.set_const();
                                declarations->insert(key.str, key.hash, declaration);

                                Statement_Initializer_Copy* initializer =
                                    parser->buffer_array.allocator()
                                        .create<Statement_Initializer_Copy>();
                                initializer->identifier = key;
                                Expression_Integer* value =
                                    parser->buffer_array.allocator().create<Expression_Integer>();
                                value->value = values.values[i];
                                initializer->value = value;
                                initializers->push(initializer);
                                declaration.v.initializer = initializer;
                            }
                        }
                    }

                    if (!type) {
                        if (!enum_type) {
                            enum_type = parser->buffer_array.allocator().create<Type_Enum>();
                            if (identifier.str.len > 0) {
                                cz::Str_Map<Type*>* types =
                                    &parser->type_stack[parser->type_stack.len() - 2];
                                types->reserve(cz::heap_allocator(), 1);
                                types->insert(identifier.str, identifier.hash, enum_type);
                            }
                        }

                        destroy_values = false;
                        enum_type->values = values;
                        enum_type->flags = flags;

                        base_type->set_type(enum_type);
                    } else {
                        base_type->set_type(*type);
                    }
                    break;
                } else {
                    Type** type = lookup_type(parser, identifier);
                    if (type) {
                        if ((*type)->tag != Type::Enum) {
                            context->report_error(identifier_span, identifier_source_span, "Type `",
                                                  identifier.str, "` is not an enum");
                        }
                        base_type->set_type(*type);
                    } else {
                        Type_Enum* enum_type = parser->buffer_array.allocator().create<Type_Enum>();
                        enum_type->values = {};
                        enum_type->flags = 0;
                        cz::Str_Map<Type*>* types = &parser->type_stack.last();
                        types->reserve(cz::heap_allocator(), 1);
                        types->insert(identifier.str, identifier.hash, enum_type);
                        base_type->set_type(enum_type);
                    }
                    break;
                }
            }

            case Token::Identifier: {
                if (base_type->get_type() || numeric_base.flags) {
                    goto stop_processing_tokens;
                }

                Declaration* declaration = lookup_declaration(parser, pair.token.v.identifier);
                TypeP* type = lookup_typedef(parser, pair.token.v.identifier);
                if (declaration) {
                    if (type) {
                        Type* t = type->get_type();
                        if (t->tag == Type::Enum) {
                            context->report_error(
                                pair.token.span, pair.source_span,
                                "Variable cannot be used as a type.  Hint: add the tag `enum`");
                        } else if (t->tag == Type::Struct) {
                            context->report_error(
                                pair.token.span, pair.source_span,
                                "Variable cannot be used as a type.  Hint: add the tag `struct`");
                        } else if (t->tag == Type::Union) {
                            context->report_error(
                                pair.token.span, pair.source_span,
                                "Variable cannot be used as a type.  Hint: add the tag `union`");
                        } else {
                            // Todo: add hint about spelling out the type
                            context->report_error(pair.token.span, pair.source_span,
                                                  "Variable cannot be used as a type.");
                        }
                    } else {
                        context->report_error(pair.token.span, pair.source_span,
                                              "Variable cannot be used as a type");
                    }
                    return {Result::ErrorInvalidInput};
                }

                next_token_after_peek(parser);

                if (!type) {
                    context->report_error(pair.token.span, pair.source_span, "Undefined type `",
                                          pair.token.v.identifier.str, "`");
                    Type** tagged_type = lookup_type(parser, pair.token.v.identifier);
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

#define CASE_NUMERIC_KEYWORD(CASE)                                                     \
    case Token::CASE:                                                                  \
        next_token_after_peek(parser);                                                 \
        if (numeric_base.flags & Numeric_Base::CASE) {                                 \
            context->report_error(pair.token.span, pair.source_span, "`", pair.token,  \
                                  "` has already been applied to the type");           \
        } else {                                                                       \
            numeric_base.flags |= Numeric_Base::CASE;                                  \
            numeric_base_spans[Numeric_Base::CASE##_Index * 2] = pair.token.span;      \
            numeric_base_spans[Numeric_Base::CASE##_Index * 2 + 1] = pair.source_span; \
        }                                                                              \
        break

                CASE_NUMERIC_KEYWORD(Char);
                CASE_NUMERIC_KEYWORD(Float);
                CASE_NUMERIC_KEYWORD(Double);
                CASE_NUMERIC_KEYWORD(Int);
                CASE_NUMERIC_KEYWORD(Short);
                CASE_NUMERIC_KEYWORD(Signed);
                CASE_NUMERIC_KEYWORD(Unsigned);

            case Token::Long: {
                next_token_after_peek(parser);
                if (numeric_base.flags & Numeric_Base::Long_Long) {
                    context->report_error(pair.token.span, pair.source_span,
                                          "Type cannot be made `long long long`");
                } else if (numeric_base.flags & Numeric_Base::Long) {
                    numeric_base.flags |= Numeric_Base::Long_Long;
                } else {
                    numeric_base.flags |= Numeric_Base::Long;
                }
                break;
            }

            case Token::Void:
                next_token_after_peek(parser);
                base_type->set_type(parser->type_void);
                break;

            case Token::Const:
                next_token_after_peek(parser);
                parse_const(context, base_type, pair);
                break;

            case Token::Volatile:
                next_token_after_peek(parser);
                parse_volatile(context, base_type, pair);
                break;

            default:
                goto stop_processing_tokens;
        }
    }

stop_processing_tokens:
    if (numeric_base.flags) {
        if ((numeric_base.flags & Numeric_Base::Signed) &&
            (numeric_base.flags & Numeric_Base::Unsigned)) {
            context->report_error(numeric_base_spans[Numeric_Base::Unsigned_Index * 2],
                                  numeric_base_spans[Numeric_Base::Unsigned_Index * 2 + 1],
                                  "Cannot be both signed and unsigned");
            context->report_error(numeric_base_spans[Numeric_Base::Signed_Index * 2],
                                  numeric_base_spans[Numeric_Base::Signed_Index * 2 + 1],
                                  "Cannot be both signed and unsigned");
            base_type->set_type(parser->type_error);
            return Result::ok();
        }

        if (numeric_base.flags & Numeric_Base::Char) {
            if (numeric_base.flags & Numeric_Base::Unsigned) {
                base_type->set_type(parser->type_unsigned_char);
            } else if (numeric_base.flags & Numeric_Base::Signed) {
                base_type->set_type(parser->type_signed_char);
            } else {
                base_type->set_type(parser->type_char);
            }

            if (numeric_base.flags & Numeric_Base::Double) {
                context->report_error(numeric_base_spans[Numeric_Base::Char_Index * 2],
                                      numeric_base_spans[Numeric_Base::Char_Index * 2 + 1],
                                      "Cannot be both `char` and `double`");
                context->report_error(numeric_base_spans[Numeric_Base::Double_Index * 2],
                                      numeric_base_spans[Numeric_Base::Double_Index * 2 + 1],
                                      "Cannot be both `char` and `double`");
                base_type->set_type(parser->type_error);
            }
            if (numeric_base.flags & Numeric_Base::Float) {
                context->report_error(numeric_base_spans[Numeric_Base::Char_Index * 2],
                                      numeric_base_spans[Numeric_Base::Char_Index * 2 + 1],
                                      "Cannot be both `char` and `float`");
                context->report_error(numeric_base_spans[Numeric_Base::Float_Index * 2],
                                      numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                                      "Cannot be both `char` and `float`");
                base_type->set_type(parser->type_error);
            }
            if (numeric_base.flags & Numeric_Base::Int) {
                context->report_error(numeric_base_spans[Numeric_Base::Char_Index * 2],
                                      numeric_base_spans[Numeric_Base::Char_Index * 2 + 1],
                                      "Cannot be both `char` and `int`");
                context->report_error(numeric_base_spans[Numeric_Base::Int_Index * 2],
                                      numeric_base_spans[Numeric_Base::Int_Index * 2 + 1],
                                      "Cannot be both `char` and `int`");
                base_type->set_type(parser->type_error);
            }
            if (numeric_base.flags & Numeric_Base::Short) {
                context->report_error(numeric_base_spans[Numeric_Base::Char_Index * 2],
                                      numeric_base_spans[Numeric_Base::Char_Index * 2 + 1],
                                      "Cannot be both `char` and `short`");
                context->report_error(numeric_base_spans[Numeric_Base::Short_Index * 2],
                                      numeric_base_spans[Numeric_Base::Short_Index * 2 + 1],
                                      "Cannot be both `char` and `short`");
                base_type->set_type(parser->type_error);
            }
            if (numeric_base.flags & Numeric_Base::Long) {
                context->report_error(numeric_base_spans[Numeric_Base::Char_Index * 2],
                                      numeric_base_spans[Numeric_Base::Char_Index * 2 + 1],
                                      "Cannot be both `char` and `long`");
                context->report_error(numeric_base_spans[Numeric_Base::Long_Index * 2],
                                      numeric_base_spans[Numeric_Base::Long_Index * 2 + 1],
                                      "Cannot be both `char` and `long`");
                base_type->set_type(parser->type_error);
            }
            if (numeric_base.flags & Numeric_Base::Long_Long) {
                context->report_error(numeric_base_spans[Numeric_Base::Char_Index * 2],
                                      numeric_base_spans[Numeric_Base::Char_Index * 2 + 1],
                                      "Cannot be both `char` and `long long`");
                context->report_error(numeric_base_spans[Numeric_Base::Long_Long_Index * 2],
                                      numeric_base_spans[Numeric_Base::Long_Long_Index * 2 + 1],
                                      "Cannot be both `char` and `long long`");
                base_type->set_type(parser->type_error);
            }
            return Result::ok();
        }

        if (numeric_base.flags & Numeric_Base::Double) {
            if (numeric_base.flags & Numeric_Base::Long) {
                base_type->set_type(parser->type_long_double);
            } else {
                base_type->set_type(parser->type_double);
            }

            if (numeric_base.flags & Numeric_Base::Signed) {
                context->report_error(
                    numeric_base_spans[Numeric_Base::Float_Index * 2],
                    numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                    "Cannot be both `double` and `signed`.  Hint: removed the keyword `signed`.");
                context->report_error(
                    numeric_base_spans[Numeric_Base::Double_Index * 2],
                    numeric_base_spans[Numeric_Base::Double_Index * 2 + 1],
                    "Cannot be both `double` and `signed`.  Hint: remove the keyword `signed`.");
            }
            if (numeric_base.flags & Numeric_Base::Unsigned) {
                context->report_error(numeric_base_spans[Numeric_Base::Double_Index * 2],
                                      numeric_base_spans[Numeric_Base::Double_Index * 2 + 1],
                                      "Cannot be both `double` and `unsigned`");
                context->report_error(numeric_base_spans[Numeric_Base::Unsigned_Index * 2],
                                      numeric_base_spans[Numeric_Base::Unsigned_Index * 2 + 1],
                                      "Cannot be both `double` and `unsigned`");
            }
            if (numeric_base.flags & Numeric_Base::Float) {
                context->report_error(numeric_base_spans[Numeric_Base::Double_Index * 2],
                                      numeric_base_spans[Numeric_Base::Double_Index * 2 + 1],
                                      "Cannot be both `double` and `float`");
                context->report_error(numeric_base_spans[Numeric_Base::Float_Index * 2],
                                      numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                                      "Cannot be both `double` and `float`");
                base_type->set_type(parser->type_double);
            }
            if (numeric_base.flags & Numeric_Base::Int) {
                context->report_error(numeric_base_spans[Numeric_Base::Double_Index * 2],
                                      numeric_base_spans[Numeric_Base::Double_Index * 2 + 1],
                                      "Cannot be both `double` and `int`");
                context->report_error(numeric_base_spans[Numeric_Base::Int_Index * 2],
                                      numeric_base_spans[Numeric_Base::Int_Index * 2 + 1],
                                      "Cannot be both `double` and `int`");
                base_type->set_type(parser->type_error);
            }
            if (numeric_base.flags & Numeric_Base::Short) {
                context->report_error(
                    numeric_base_spans[Numeric_Base::Double_Index * 2],
                    numeric_base_spans[Numeric_Base::Double_Index * 2 + 1],
                    "Cannot be both `double` and `short`.  Perhaps you meant `float`?");
                context->report_error(
                    numeric_base_spans[Numeric_Base::Short_Index * 2],
                    numeric_base_spans[Numeric_Base::Short_Index * 2 + 1],
                    "Cannot be both `double` and `short`.  Perhaps you meant `float`?");
                base_type->set_type(parser->type_float);
            }
            if (numeric_base.flags & Numeric_Base::Long_Long) {
                context->report_error(
                    numeric_base_spans[Numeric_Base::Double_Index * 2],
                    numeric_base_spans[Numeric_Base::Double_Index * 2 + 1],
                    "Cannot be both `double` and `long long`.  Perhaps you meant `long double`?");
                context->report_error(
                    numeric_base_spans[Numeric_Base::Long_Long_Index * 2],
                    numeric_base_spans[Numeric_Base::Long_Long_Index * 2 + 1],
                    "Cannot be both `double` and `long long`.  Perhaps you meant `long double`?");
                base_type->set_type(parser->type_long_double);
            }
            return Result::ok();
        }

        if (numeric_base.flags & Numeric_Base::Float) {
            base_type->set_type(parser->type_float);

            if (numeric_base.flags & Numeric_Base::Signed) {
                context->report_error(
                    numeric_base_spans[Numeric_Base::Float_Index * 2],
                    numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                    "Cannot be both `float` and `signed`.  Hint: removed the keyword `signed`.");
                context->report_error(
                    numeric_base_spans[Numeric_Base::Double_Index * 2],
                    numeric_base_spans[Numeric_Base::Double_Index * 2 + 1],
                    "Cannot be both `float` and `signed`.  Hint: remove the keyword `signed`.");
            }
            if (numeric_base.flags & Numeric_Base::Unsigned) {
                context->report_error(numeric_base_spans[Numeric_Base::Float_Index * 2],
                                      numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                                      "Cannot be both `float` and `unsigned`");
                context->report_error(numeric_base_spans[Numeric_Base::Unsigned_Index * 2],
                                      numeric_base_spans[Numeric_Base::Unsigned_Index * 2 + 1],
                                      "Cannot be both `float` and `unsigned`");
            }
            if (numeric_base.flags & Numeric_Base::Double) {
                context->report_error(numeric_base_spans[Numeric_Base::Float_Index * 2],
                                      numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                                      "Cannot be both `float` and `double`");
                context->report_error(numeric_base_spans[Numeric_Base::Double_Index * 2],
                                      numeric_base_spans[Numeric_Base::Double_Index * 2 + 1],
                                      "Cannot be both `float` and `double`");
                base_type->set_type(parser->type_double);
            }
            if (numeric_base.flags & Numeric_Base::Int) {
                context->report_error(numeric_base_spans[Numeric_Base::Float_Index * 2],
                                      numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                                      "Cannot be both `float` and `int`");
                context->report_error(numeric_base_spans[Numeric_Base::Int_Index * 2],
                                      numeric_base_spans[Numeric_Base::Int_Index * 2 + 1],
                                      "Cannot be both `float` and `int`");
                base_type->set_type(parser->type_error);
            }
            if (numeric_base.flags & Numeric_Base::Short) {
                context->report_error(numeric_base_spans[Numeric_Base::Float_Index * 2],
                                      numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                                      "Cannot be both `float` and `short`");
                context->report_error(numeric_base_spans[Numeric_Base::Short_Index * 2],
                                      numeric_base_spans[Numeric_Base::Short_Index * 2 + 1],
                                      "Cannot be both `float` and `short`");
            }
            if (numeric_base.flags & Numeric_Base::Long) {
                context->report_error(
                    numeric_base_spans[Numeric_Base::Float_Index * 2],
                    numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                    "Cannot be both `float` and `long`.  Perhaps you meant `double`?");
                context->report_error(
                    numeric_base_spans[Numeric_Base::Long_Index * 2],
                    numeric_base_spans[Numeric_Base::Long_Index * 2 + 1],
                    "Cannot be both `float` and `long`.  Perhaps you meant `double`?");
                base_type->set_type(parser->type_double);
            }
            if (numeric_base.flags & Numeric_Base::Long_Long) {
                context->report_error(
                    numeric_base_spans[Numeric_Base::Float_Index * 2],
                    numeric_base_spans[Numeric_Base::Float_Index * 2 + 1],
                    "Cannot be both `float` and `long long`.  Perhaps you meant `long double`?");
                context->report_error(
                    numeric_base_spans[Numeric_Base::Long_Long_Index * 2],
                    numeric_base_spans[Numeric_Base::Long_Long_Index * 2 + 1],
                    "Cannot be both `float` and `long long`.  Perhaps you meant `long double`?");
                base_type->set_type(parser->type_long_double);
            }
            return Result::ok();
        }

        if ((numeric_base.flags & Numeric_Base::Short) &&
            (numeric_base.flags & Numeric_Base::Long)) {
            context->report_error(numeric_base_spans[Numeric_Base::Short_Index * 2],
                                  numeric_base_spans[Numeric_Base::Short_Index * 2 + 1],
                                  "Cannot be both `short` and `long`");
            context->report_error(numeric_base_spans[Numeric_Base::Long_Index * 2],
                                  numeric_base_spans[Numeric_Base::Long_Index * 2 + 1],
                                  "Cannot be both `short` and `long`");
            base_type->set_type(parser->type_error);
            return Result::ok();
        }

        if (numeric_base.flags & Numeric_Base::Long_Long) {
            if (numeric_base.flags & Numeric_Base::Unsigned) {
                base_type->set_type(parser->type_unsigned_long_long);
            } else {
                base_type->set_type(parser->type_signed_long_long);
            }
        } else if (numeric_base.flags & Numeric_Base::Long) {
            if (numeric_base.flags & Numeric_Base::Unsigned) {
                base_type->set_type(parser->type_unsigned_long);
            } else {
                base_type->set_type(parser->type_signed_long);
            }
        } else if (numeric_base.flags & Numeric_Base::Short) {
            if (numeric_base.flags & Numeric_Base::Unsigned) {
                base_type->set_type(parser->type_unsigned_short);
            } else {
                base_type->set_type(parser->type_signed_short);
            }
        } else {
            if (numeric_base.flags & Numeric_Base::Unsigned) {
                base_type->set_type(parser->type_unsigned_int);
            } else {
                base_type->set_type(parser->type_signed_int);
            }
        }
        return Result::ok();
    }

    if (!base_type->get_type()) {
        context->report_error(pair.token.span, pair.source_span, "Expected type here");
        return {Result::ErrorInvalidInput};
    }

    return Result::ok();
}

Result parse_declaration_(Context* context,
                          Parser* parser,
                          cz::Vector<Statement*>* initializers,
                          uint32_t flags) {
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
    Result result = parse_base_type(context, parser, initializers, &base_type);
    if (result.type != Result::Success) {
        return result;
    }

    Token_Source_Span_Pair pair;
    result = peek_token(context, parser, &pair);
    CZ_TRY_VAR(result);
    if (result.type == Result::Success && pair.token.type == Token::Semicolon) {
        next_token_after_peek(parser);
        return Result::ok();
    }

    while (1) {
        TypeP type = base_type;
        Hashed_Str identifier = {};
        cz::Slice<cz::Str> parameter_names;
        CZ_TRY(parse_declaration_identifier_and_type(context, parser, initializers, &identifier,
                                                     &type, nullptr, &parameter_names));

        if (identifier.str.len > 0) {
            bool force_terminate = false;
            Declaration declaration = {};
            declaration.type = type;
            declaration.flags = flags;
            CZ_TRY(parse_declaration_initializer(context, parser, declaration, identifier,
                                                 initializers, parameter_names, &force_terminate));
            if (force_terminate) {
                return Result::ok();
            }
        }

        Span previous_span = pair.token.span;
        Span previous_source_span = pair.source_span;
        result = next_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(previous_span, previous_source_span,
                                  "Expected ';' to end declaration here");
            return {Result::ErrorInvalidInput};
        }

        if (pair.token.type == Token::Comma) {
            continue;
        } else if (pair.token.type == Token::Semicolon) {
            break;
        } else {
            context->report_error(pair.token.span, pair.source_span,
                                  "Expected ';' to end declaration here");
            return {Result::ErrorInvalidInput};
        }
    }

    return Result::ok();
}

Result parse_declaration(Context* context, Parser* parser, cz::Vector<Statement*>* initializers) {
    Token_Source_Span_Pair pair;
    Result result = peek_token(context, parser, &pair);
    CZ_TRY_VAR(result);
    if (result.type == Result::Success && pair.token.type == Token::Typedef) {
        next_token_after_peek(parser);

        size_t len = initializers->len();
        parser->declaration_stack.reserve(cz::heap_allocator(), 1);
        parser->declaration_stack.push({});
        CZ_DEFER({
            parser->declaration_stack.last().drop(cz::heap_allocator());
            parser->declaration_stack.pop();
        });

        result = parse_declaration_(context, parser, initializers, 0);

        cz::Str_Map<TypeP>* typedefs = &parser->typedef_stack.last();
        typedefs->reserve(cz::heap_allocator(), initializers->len() - len);
        for (size_t i = len; i < initializers->len(); ++i) {
            Statement* init = (*initializers)[i];
            if (init->tag != Statement::Initializer_Default) {
                context->report_error(pair.token.span, pair.source_span,
                                      "Typedef cannot have initializer");
            }

            Statement_Initializer* in = (Statement_Initializer*)init;
            Declaration* declaration =
                parser->declaration_stack.last().get(in->identifier.str, in->identifier.hash);
            CZ_DEBUG_ASSERT(declaration);
            if (!typedefs->get(in->identifier.str, in->identifier.hash)) {
                typedefs->insert(in->identifier.str, in->identifier.hash, declaration->type);
            } else {
                context->report_error(pair.token.span, pair.source_span, "Typedef `",
                                      in->identifier.str, "` has already been created");
            }
        }

        return Result::ok();
    } else {
        uint32_t flags = 0;
        while (result.type == Result::Success) {
            switch (pair.token.type) {
                case Token::Extern: {
                    if (flags & Declaration::Extern) {
                        context->report_error(pair.token.span, pair.source_span,
                                              "Declaration has already been declared `extern`");
                    }
                    flags |= Declaration::Extern;
                } break;

                case Token::Static: {
                    if (flags & Declaration::Static) {
                        context->report_error(pair.token.span, pair.source_span,
                                              "Declaration has already been declared `static`");
                    }
                    flags |= Declaration::Static;
                } break;

                default:
                    goto pd;
            }

            next_token_after_peek(parser);
            result = peek_token(context, parser, &pair);
            CZ_TRY_VAR(result);
        }

    pd:
        return parse_declaration_(context, parser, initializers, flags);
    }
}

Result parse_declaration_or_statement(Context* context,
                                      Parser* parser,
                                      cz::Vector<Statement*>* statements,
                                      Declaration_Or_Statement* which) {
    Token_Source_Span_Pair pair;
    Result result = peek_token(context, parser, &pair);
    if (result.type != Result::Success) {
        return result;
    }

    switch (pair.token.type) {
        case Token::Char:
        case Token::Double:
        case Token::Float:
        case Token::Int:
        case Token::Long:
        case Token::Short:
        case Token::Void:
        case Token::Extern:
        case Token::Enum:
        case Token::Struct:
        case Token::Union:
        case Token::Typedef:
        case Token::Const:
        case Token::Volatile: {
            *which = Declaration_Or_Statement::Declaration;
            return parse_declaration(context, parser, statements);
        }

        case Token::Identifier: {
            Declaration* declaration = lookup_declaration(parser, pair.token.v.identifier);
            TypeP* type = lookup_typedef(parser, pair.token.v.identifier);
            if (declaration) {
                *which = Declaration_Or_Statement::Statement;

                Statement* statement;
                result = parse_statement(context, parser, &statement);
                if (result.type == Result::Success) {
                    statements->reserve(cz::heap_allocator(), 1);
                    statements->push(statement);
                }
                return result;
            } else if (type) {
                *which = Declaration_Or_Statement::Declaration;
                return parse_declaration(context, parser, statements);
            } else {
                context->report_error(pair.token.span, pair.source_span, "Undefined identifier `",
                                      pair.token.v.identifier.str, "`");
                return {Result::ErrorInvalidInput};
            }
        }

        default: {
            *which = Declaration_Or_Statement::Statement;

            Statement* statement;
            result = parse_statement(context, parser, &statement);
            if (result.type == Result::Success) {
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
    Token_Source_Span_Pair pair;
    Result result = next_token(context, parser, &pair);
    if (result.type != Result::Success) {
        return result;
    }

    // Parse one atom
    switch (pair.token.type) {
        case Token::Integer: {
            Expression_Integer* expression =
                parser->buffer_array.allocator().create<Expression_Integer>();
            expression->value = pair.token.v.integer.value;
            *eout = expression;
            break;
        }

        case Token::Identifier: {
            if (lookup_declaration(parser, pair.token.v.identifier)) {
                Expression_Variable* expression =
                    parser->buffer_array.allocator().create<Expression_Variable>();
                expression->variable = pair.token.v.identifier;
                *eout = expression;
                break;
            } else {
                context->report_error(pair.token.span, pair.source_span, "Undefined variable `",
                                      pair.token.v.identifier.str, "`");
                return {Result::ErrorInvalidInput};
            }
        }

        case Token::OpenParen: {
            result = parse_expression(context, parser, eout);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(pair.token.span, pair.source_span,
                                      "Unmatched parenthesis (`(`)");
                return {Result::ErrorInvalidInput};
            }

            Token_Source_Span_Pair open_paren_pair = pair;
            result = next_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(open_paren_pair.token.span, open_paren_pair.source_span,
                                      "Unmatched parenthesis (`(`)");
                return {Result::ErrorInvalidInput};
            }
            if (pair.token.type != Token::CloseParen) {
                context->report_error(pair.token.span, pair.source_span,
                                      "Expected close parenthesis (`)`) here");
                return {Result::ErrorInvalidInput};
            }
            break;
        }

        default:
            context->report_error(pair.token.span, pair.source_span, "Expected expression here");
            return {Result::ErrorInvalidInput};
    }

    while (1) {
        result = peek_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            return Result::ok();
        }

        int precedence;
        bool ltr = true;
        switch (pair.token.type) {
            case Token::CloseParen:
            case Token::CloseSquare:
            case Token::Semicolon:
            case Token::Colon:
                return Result::ok();

            case Token::QuestionMark: {
                precedence = 16;
                ltr = false;

                if (precedence >= max_precedence) {
                    return Result::ok();
                }

                next_token_after_peek(parser);

                Expression* then;
                result = parse_expression(context, parser, &then);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(
                        pair.token.span, pair.source_span,
                        "Expected then expression side for ternary operator here");
                    return Result::ok();
                }

                Token_Source_Span_Pair question_mark_pair = pair;
                result = next_token(context, parser, &pair);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(
                        question_mark_pair.token.span, question_mark_pair.source_span,
                        "Expected `:` and then otherwise expression side for ternary operator");
                    return Result::ok();
                }

                Expression* otherwise;
                result = parse_expression_(context, parser, &otherwise, precedence + !ltr);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(
                        pair.token.span, pair.source_span,
                        "Expected otherwise expression for ternary operator here");
                    return Result::ok();
                }

                Expression_Ternary* ternary =
                    parser->buffer_array.allocator().create<Expression_Ternary>();
                ternary->condition = *eout;
                ternary->then = then;
                ternary->otherwise = otherwise;
                *eout = ternary;
                continue;
            }

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
                context->report_error(pair.token.span, pair.source_span,
                                      "Expected binary operator here to connect expressions");
                return {Result::ErrorInvalidInput};
        }

        if (precedence >= max_precedence) {
            return Result::ok();
        }

        next_token_after_peek(parser);

        Expression* right;
        result = parse_expression_(context, parser, &right, precedence + !ltr);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(pair.token.span, pair.source_span,
                                  "Expected right side for binary operator here");
            break;
        }

        Expression_Binary* binary = parser->buffer_array.allocator().create<Expression_Binary>();
        binary->op = pair.token.type;
        binary->left = *eout;
        binary->right = right;
        *eout = binary;
    }

    return Result::ok();
}

Result parse_expression(Context* context, Parser* parser, Expression** eout) {
    return parse_expression_(context, parser, eout, 100);
}

static Result parse_block(Context* context, Parser* parser, Block* block) {
    Result result;
    Token_Source_Span_Pair open_curly_pair;
    next_token(context, parser, &open_curly_pair);

    parser->type_stack.reserve(cz::heap_allocator(), 1);
    parser->typedef_stack.reserve(cz::heap_allocator(), 1);
    parser->declaration_stack.reserve(cz::heap_allocator(), 1);
    parser->type_stack.push({});
    parser->typedef_stack.push({});
    parser->declaration_stack.push({});
    CZ_DEFER({
        drop_types(&parser->type_stack.last());
        parser->typedef_stack.last().drop(cz::heap_allocator());
        parser->declaration_stack.last().drop(cz::heap_allocator());
        parser->type_stack.pop();
        parser->typedef_stack.pop();
        parser->declaration_stack.pop();
    });

    cz::Vector<Statement*> statements = {};
    CZ_DEFER(statements.drop(cz::heap_allocator()));
    Token_Source_Span_Pair pair;
    while (1) {
        result = peek_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(open_curly_pair.token.span, open_curly_pair.source_span,
                                  "Expected end of block to match start of block (`{`) here");
            return {Result::ErrorInvalidInput};
        }
        if (pair.token.type == Token::CloseCurly) {
            next_token_after_peek(parser);
            goto finish_block;
        }

        Declaration_Or_Statement which;
        CZ_TRY(parse_declaration_or_statement(context, parser, &statements, &which));

        if (which == Declaration_Or_Statement::Statement) {
            break;
        }
    }

    while (1) {
        result = peek_token(context, parser, &pair);
        CZ_TRY_VAR(result);
        if (result.type == Result::Done) {
            context->report_error(open_curly_pair.token.span, open_curly_pair.source_span,
                                  "Expected end of block to match start of block (`{`) here");
            return {Result::ErrorInvalidInput};
        }
        if (pair.token.type == Token::CloseCurly) {
            next_token_after_peek(parser);
            goto finish_block;
        }

        Statement* statement;
        CZ_TRY(parse_statement(context, parser, &statement));

        statements.reserve(cz::heap_allocator(), 1);
        statements.push(statement);
    }

finish_block:
    block->statements = parser->buffer_array.allocator().duplicate(statements.as_slice());
    return Result::ok();
}

Result parse_statement(Context* context, Parser* parser, Statement** sout) {
    Token_Source_Span_Pair pair;
    Result result = peek_token(context, parser, &pair);
    if (result.type != Result::Success) {
        return result;
    }

    switch (pair.token.type) {
        case Token::OpenCurly: {
            Block block;
            CZ_TRY(parse_block(context, parser, &block));

            Statement_Block* statement = parser->buffer_array.allocator().create<Statement_Block>();
            statement->block = block;
            *sout = statement;
            return Result::ok();
        }

        case Token::While: {
            Token_Source_Span_Pair while_pair = pair;
            next_token_after_peek(parser);

            result = next_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(while_pair.token.span, while_pair.source_span,
                                      "Expected open parenthesis here");
                return {Result::ErrorInvalidInput};
            }
            if (pair.token.type != Token::OpenParen) {
                context->report_error(pair.token.span, pair.source_span,
                                      "Expected open parenthesis here");
                return {Result::ErrorInvalidInput};
            }

            Expression* condition;
            result = parse_expression(context, parser, &condition);
            if (result.type == Result::Done) {
                context->report_error(while_pair.token.span, while_pair.source_span,
                                      "Expected condition expression here");
                return {Result::ErrorInvalidInput};
            }

            result = next_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(while_pair.token.span, while_pair.source_span,
                                      "Expected `)` to end condition expression");
                return {Result::ErrorInvalidInput};
            }
            if (pair.token.type != Token::CloseParen) {
                context->report_error(pair.token.span, pair.source_span,
                                      "Expected `)` here to end condition expression");
                return {Result::ErrorInvalidInput};
            }

            Statement* body;
            result = parse_statement(context, parser, &body);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(while_pair.token.span, while_pair.source_span,
                                      "Expected body statement");
                return {Result::ErrorInvalidInput};
            }

            Statement_While* statement = parser->buffer_array.allocator().create<Statement_While>();
            statement->condition = condition;
            statement->body = body;
            *sout = statement;
            return Result::ok();
        }

        case Token::For: {
            Token_Source_Span_Pair for_pair = pair;
            next_token_after_peek(parser);

            result = next_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_pair.token.span, for_pair.source_span,
                                      "Expected open parenthesis here");
                return {Result::ErrorInvalidInput};
            }
            if (pair.token.type != Token::OpenParen) {
                context->report_error(pair.token.span, pair.source_span,
                                      "Expected open parenthesis here");
                return {Result::ErrorInvalidInput};
            }

            result = peek_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_pair.token.span, for_pair.source_span,
                                      "Expected initializer or `;`");
                return {Result::ErrorInvalidInput};
            }
            Expression* initializer = nullptr;
            if (pair.token.type == Token::Semicolon) {
                next_token_after_peek(parser);
            } else {
                CZ_TRY(parse_expression(context, parser, &initializer));

                result = next_token(context, parser, &pair);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(for_pair.token.span, for_pair.source_span,
                                          "Expected `;` to end initializer expression");
                    return {Result::ErrorInvalidInput};
                }
                if (pair.token.type != Token::Semicolon) {
                    context->report_error(pair.token.span, pair.source_span,
                                          "Expected `;` here to end initializer expression");
                    return {Result::ErrorInvalidInput};
                }
            }

            result = peek_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_pair.token.span, for_pair.source_span,
                                      "Expected condition expression or `;` here");
                return {Result::ErrorInvalidInput};
            }
            Expression* condition = nullptr;
            if (pair.token.type == Token::Semicolon) {
                next_token_after_peek(parser);
            } else {
                CZ_TRY(parse_expression(context, parser, &condition));

                result = next_token(context, parser, &pair);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(for_pair.token.span, for_pair.source_span,
                                          "Expected `;` to end condition expression");
                    return {Result::ErrorInvalidInput};
                }
                if (pair.token.type != Token::Semicolon) {
                    context->report_error(pair.token.span, pair.source_span,
                                          "Expected `;` here to end condition expression");
                    return {Result::ErrorInvalidInput};
                }
            }

            result = peek_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_pair.token.span, for_pair.source_span,
                                      "Expected increment expression or `)`");
                return {Result::ErrorInvalidInput};
            }
            Expression* increment = nullptr;
            if (pair.token.type == Token::CloseParen) {
                next_token_after_peek(parser);
            } else {
                CZ_TRY(parse_expression(context, parser, &increment));

                result = next_token(context, parser, &pair);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done) {
                    context->report_error(for_pair.token.span, for_pair.source_span,
                                          "Expected `)` to end increment expression");
                    return {Result::ErrorInvalidInput};
                }
                if (pair.token.type != Token::CloseParen) {
                    context->report_error(pair.token.span, pair.source_span,
                                          "Expected `)` here to end increment expression");
                    return {Result::ErrorInvalidInput};
                }
            }

            Statement* body;
            result = parse_statement(context, parser, &body);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(for_pair.token.span, for_pair.source_span,
                                      "Expected body statement");
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

        case Token::Return: {
            next_token_after_peek(parser);
            result = peek_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(pair.token.span, pair.source_span,
                                      "Expected semicolon here to end expression statement");
                return {Result::ErrorInvalidInput};
            }

            Expression* value;
            if (pair.token.type == Token::Semicolon) {
                value = nullptr;
            } else {
                CZ_TRY(parse_expression(context, parser, &value));

                previous_token(parser, &pair);
                result = peek_token(context, parser, &pair);
                CZ_TRY_VAR(result);
                if (result.type == Result::Done || pair.token.type != Token::Semicolon) {
                    context->report_error(pair.token.span, pair.source_span,
                                          "Expected semicolon here to end expression statement");
                    return {Result::ErrorInvalidInput};
                }
            }
            next_token_after_peek(parser);

            Statement_Return* statement =
                parser->buffer_array.allocator().create<Statement_Return>();
            statement->o_value = value;
            *sout = statement;
            return Result::ok();
        }

        default: {
            Expression* expression;
            CZ_TRY(parse_expression(context, parser, &expression));

            Token_Source_Span_Pair previous;
            previous_token(parser, &previous);
            result = next_token(context, parser, &pair);
            CZ_TRY_VAR(result);
            if (result.type == Result::Done) {
                context->report_error(pair.token.span, pair.source_span,
                                      "Expected semicolon here to end expression statement");
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
