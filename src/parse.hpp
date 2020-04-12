#pragma once

#include <cz/buffer_array.hpp>
#include <cz/str_map.hpp>
#include <cz/vector.hpp>
#include "lex.hpp"
#include "preprocess.hpp"

namespace red {
namespace parse {

struct Declaration;
struct Type_Pointer;

struct alignas(4) Type {
    enum Tag {
        Builtin_Char,
        Builtin_Double,
        Builtin_Float,
        Builtin_Int,
        Builtin_Long,
        Builtin_Short,
        Builtin_Void,
        Enum,
        Struct,
        Union,
        Pointer,
    };

    Type(Tag tag) : tag(tag) {}

    Tag tag;
};

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

struct Type_Enum : Type {
    Type_Enum() : Type(Enum) {}

    cz::Slice<cz::Str> names;
    cz::Slice<int64_t> values;
};

struct Type_Struct : Type {
    Type_Struct() : Type(Struct) {}

    cz::Slice<cz::Str> names;
    cz::Slice<Declaration> declarations;
};

struct Type_Union : Type {
    Type_Union() : Type(Union) {}

    cz::Slice<cz::Str> names;
    cz::Slice<Declaration> declarations;
};

struct Type_Pointer : Type {
    Type_Pointer() : Type(Pointer) {}

    TypeP inner;
};

struct Expression {
    enum Tag {
        Integer,
        Binary,
    };

    Tag tag;

    Expression(Tag tag) : tag(tag) {}
};

struct Declaration {
    TypeP type;
    Type_Pointer* o_type_pointer;
    Expression* o_value;
};

struct Parser {
    cpp::Preprocessor preprocessor;
    lex::Lexer lexer;

    cz::Vector<cz::Str_Map<Type*> > type_stack;
    cz::Vector<cz::Str_Map<TypeP> > typedef_stack;
    cz::Vector<cz::Str_Map<Declaration> > declaration_stack;

    cz::Buffer_Array type_buffer_array;
    Type* type_char;
    Type* type_double;
    Type* type_float;
    Type* type_int;
    Type* type_long;
    Type* type_short;
    Type* type_void;

    Token back;

    void init();
    void drop();
};

Result parse_declaration(Context* context, Parser* parser);
Result parse_expression(Context* context, Parser* parser, Expression** expression);

}
}