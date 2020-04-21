#pragma once

#include <cz/buffer_array.hpp>
#include <cz/str_map.hpp>
#include <cz/vector.hpp>
#include "lex.hpp"
#include "preprocess.hpp"
#include "token_source_span_pair.hpp"

namespace red {
namespace parse {

struct Declaration;
struct Type_Pointer;

struct alignas(4) Type {
    enum Tag {
        Builtin_Char,
        Builtin_Signed_Char,
        Builtin_Unsigned_Char,

        Builtin_Float,
        Builtin_Double,
        Builtin_Long_Double,

        Builtin_Signed_Short,
        Builtin_Signed_Int,
        Builtin_Signed_Long,
        Builtin_Signed_Long_Long,
        Builtin_Unsigned_Short,
        Builtin_Unsigned_Int,
        Builtin_Unsigned_Long,
        Builtin_Unsigned_Long_Long,

        Builtin_Void,
        Builtin_Error,

        Enum,
        Struct,
        Union,
        Pointer,
        Array,
        Function,
    };

    Type(Tag tag) : tag(tag) {}

    Tag tag;
};

bool get_type_size_alignment(const Type* type, size_t* size, size_t* alignment);

struct TypeP {
    uintptr_t value;

    void clear() { value = 0; }
    Type* get_type() const { return (Type*)(value & ~3); }
    void set_type(Type* type) { value = ((uintptr_t)type | (value & 3)); }
    void merge_typedef(TypeP type) { value = (type.value | (value & 3)); }
    bool is_const() const { return value & 1; }
    bool is_volatile() const { return value & 2; }
    void set_const() { value |= 1; }
    void set_volatile() { value |= 2; }
};

struct Type_Definition {
    TypeP type;
    Span span;
};

struct Type_Enum : Type {
    Type_Enum() : Type(Enum) {}

    Span span;
    cz::Str_Map<int64_t> values;

    enum Flags : uint32_t {
        Defined = 1,
    };
    uint32_t flags;
};

struct Type_Composite : Type {
    Type_Composite(Tag tag) : Type(tag) {}

    Span span;
    cz::Str_Map<Type*> types;
    cz::Str_Map<Type_Definition> typedefs;
    cz::Str_Map<Declaration> declarations;

    size_t size;
    size_t alignment;

    enum Flags : uint32_t {
        Defined = 1,
    };
    uint32_t flags;
};

struct Type_Struct : Type_Composite {
    Type_Struct() : Type_Composite(Struct) {}

    cz::Slice<struct Statement*> initializers;
};

struct Type_Union : Type_Composite {
    Type_Union() : Type_Composite(Union) {}
};

struct Type_Pointer : Type {
    Type_Pointer() : Type(Pointer) {}

    TypeP inner;
};

struct Type_Array : Type {
    Type_Array() : Type(Array) {}

    TypeP inner;
    struct Expression* o_length;
};

struct Type_Function : Type {
    Type_Function() : Type(Function) {}

    TypeP return_type;
    cz::Slice<TypeP> parameter_types;
    bool has_varargs;
};

struct Expression {
    enum Tag {
        Integer,
        Variable,
        Binary,
        Ternary,
        Cast,
        Sizeof_Type,
        Sizeof_Expression,
        Function_Call,
        Index,
        Address_Of,
        Dereference,
        Bit_Not,
        Logical_Not,
        Member_Access,
        Dereference_Member_Access,
        Pre_Increment,
        Post_Increment,
        Pre_Decrement,
        Post_Decrement,
    };

    Span span;
    Tag tag;

    Expression(Tag tag) : tag(tag) {}
};

struct Expression_Integer : Expression {
    Expression_Integer() : Expression(Integer) {}

    uint64_t value;
};

struct Expression_Variable : Expression {
    Expression_Variable() : Expression(Variable) {}

    Hashed_Str variable;
};

struct Expression_Binary : Expression {
    Expression_Binary() : Expression(Binary) {}

    Token::Type op;
    Expression* left;
    Expression* right;
};

struct Expression_Ternary : Expression {
    Expression_Ternary() : Expression(Ternary) {}

    Expression* condition;
    Expression* then;
    Expression* otherwise;
};

struct Expression_Cast : Expression {
    Expression_Cast() : Expression(Cast) {}

    TypeP type;
    Expression* value;
};

struct Expression_Sizeof_Type : Expression {
    Expression_Sizeof_Type() : Expression(Sizeof_Type) {}

    TypeP type;
};

struct Expression_Sizeof_Expression : Expression {
    Expression_Sizeof_Expression() : Expression(Sizeof_Expression) {}

    Expression* expression;
};

struct Expression_Function_Call : Expression {
    Expression_Function_Call() : Expression(Function_Call) {}

    Expression* function;
    cz::Slice<Expression*> arguments;
};

struct Expression_Index : Expression {
    Expression_Index() : Expression(Index) {}

    Expression* array;
    Expression* index;
};

struct Expression_Address_Of : Expression {
    Expression_Address_Of() : Expression(Address_Of) {}

    Expression* value;
};

struct Expression_Dereference : Expression {
    Expression_Dereference() : Expression(Dereference) {}

    Expression* value;
};

struct Expression_Bit_Not : Expression {
    Expression_Bit_Not() : Expression(Bit_Not) {}

    Expression* value;
};

struct Expression_Logical_Not : Expression {
    Expression_Logical_Not() : Expression(Logical_Not) {}

    Expression* value;
};

struct Expression_Member_Access : Expression {
    Expression_Member_Access() : Expression(Member_Access) {}

    Expression* object;
    Hashed_Str field;
};

struct Expression_Dereference_Member_Access : Expression {
    Expression_Dereference_Member_Access() : Expression(Dereference_Member_Access) {}

    Expression* pointer;
    Hashed_Str field;
};

struct Expression_Pre_Increment : Expression {
    Expression_Pre_Increment() : Expression(Pre_Increment) {}

    Expression* value;
};

struct Expression_Post_Increment : Expression {
    Expression_Post_Increment() : Expression(Post_Increment) {}

    Expression* value;
};

struct Expression_Pre_Decrement : Expression {
    Expression_Pre_Decrement() : Expression(Pre_Decrement) {}

    Expression* value;
};

struct Expression_Post_Decrement : Expression {
    Expression_Post_Decrement() : Expression(Post_Decrement) {}

    Expression* value;
};

struct Statement {
    enum Tag {
        Expression,
        Block,
        For,
        While,
        Return,
        Empty,
        If,
        Continue,
        Break,
        Initializer_Default,
        Initializer_Copy,
    };

    Span span;
    Tag tag;

    Statement(Tag tag) : tag(tag) {}
};

struct Statement_Expression : Statement {
    Statement_Expression() : Statement(Expression) {}

    struct Expression* expression;
};

struct Block {
    cz::Slice<Statement*> statements;
};

struct Statement_Block : Statement {
    Statement_Block() : Statement(Block) {}

    struct Block block;
};

struct Statement_For : Statement {
    Statement_For() : Statement(For) {}

    struct Expression* initializer;
    struct Expression* condition;
    struct Expression* increment;
    Statement* body;
};

struct Statement_While : Statement {
    Statement_While() : Statement(While) {}

    struct Expression* condition;
    Statement* body;
};

struct Statement_Return : Statement {
    Statement_Return() : Statement(Return) {}

    struct Expression* o_value;
};

struct Statement_Empty : Statement {
    Statement_Empty() : Statement(Empty) {}
};

struct Statement_If : Statement {
    Statement_If() : Statement(If) {}

    struct Expression* condition;
    Statement* then;
    Statement* otherwise;
};

struct Statement_Continue : Statement {
    Statement_Continue() : Statement(Continue) {}
};

struct Statement_Break : Statement {
    Statement_Break() : Statement(Break) {}
};

struct Statement_Initializer : Statement {
    Statement_Initializer(Tag tag) : Statement(tag) {}

    Hashed_Str identifier;
};

struct Statement_Initializer_Default : Statement_Initializer {
    Statement_Initializer_Default() : Statement_Initializer(Initializer_Default) {}
};

struct Statement_Initializer_Copy : Statement_Initializer {
    Statement_Initializer_Copy() : Statement_Initializer(Initializer_Copy) {}

    struct Expression* value;
};

struct Function_Definition {
    cz::Slice<cz::Str> parameter_names;
    Block block;
    Span block_span;
};

struct Declaration {
    Span span;
    TypeP type;
    Type_Pointer* o_type_pointer;
    union {
        Function_Definition* function_definition;
        Statement_Initializer* initializer;
    } v;

    uint32_t flags;
    enum {
        Extern = 1,
        Static = 2,
        Enum_Variant = 4,
    };
};

namespace Declaration_Or_Statement_ {
enum Declaration_Or_Statement {
    Declaration,
    Statement,
};
}
using Declaration_Or_Statement_::Declaration_Or_Statement;

struct Parser {
    pre::Preprocessor preprocessor;
    lex::Lexer lexer;

    cz::Vector<cz::Str_Map<Type*> > type_stack;
    cz::Vector<cz::Str_Map<Type_Definition> > typedef_stack;
    cz::Vector<cz::Str_Map<Declaration> > declaration_stack;

    cz::Buffer_Array buffer_array;

    Type* type_char;
    Type* type_signed_char;
    Type* type_unsigned_char;

    Type* type_float;
    Type* type_double;
    Type* type_long_double;

    Type* type_signed_short;
    Type* type_signed_int;
    Type* type_signed_long;
    Type* type_signed_long_long;
    Type* type_unsigned_short;
    Type* type_unsigned_int;
    Type* type_unsigned_long;
    Type* type_unsigned_long_long;

    Type* type_void;
    Type* type_error;

    Token_Source_Span_Pair pairs[4];
    int pair_index;

    void init();
    void drop();
};

Result parse_declaration(Context* context, Parser* parser, cz::Vector<Statement*>* initializers);
Result parse_expression(Context* context, Parser* parser, Expression** expression);
Result parse_statement(Context* context, Parser* parser, Statement** statement);
Result parse_declaration_or_statement(Context* context,
                                      Parser* parser,
                                      cz::Vector<Statement*>* statements,
                                      Declaration_Or_Statement* which);

void drop_type(Type* type);
void drop_types(cz::Str_Map<Type*>* types);

}
}
