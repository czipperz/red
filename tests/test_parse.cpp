#include "test_base.hpp"

#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include "context.hpp"
#include "file_contents.hpp"
#include "load.hpp"
#include "parse.hpp"

using namespace red;
using namespace red::parse;

static void setup(Context* context, Parser* parser, cz::Str contents) {
    context->init();

    parser->init();
    include_file_reserve(&context->files, &parser->preprocessor);
    File_Contents file_contents;
    file_contents.load_str(contents, context->files.file_array_buffer_array.allocator());
    Hashed_Str file_path = Hashed_Str::from_str("*test_file*");
    force_include_file(&context->files, &parser->preprocessor, file_path, file_contents);
}

#define SETUP(CONTENTS)                 \
    Context context = {};               \
    Parser parser = {};                 \
    setup(&context, &parser, CONTENTS); \
                                        \
    CZ_DEFER({                          \
        parser.drop();                  \
        context.destroy();              \
    })

TEST_CASE("parse_declaration type but no identifier") {
    SETUP("int;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);
    CHECK(initializers.len() == 0);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration type with identifier") {
    SETUP("int abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    REQUIRE(initializers.len() == 1);
    REQUIRE(initializers[0]);
    REQUIRE(initializers[0]->tag == Statement::Initializer_Default);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration two variables same type") {
    SETUP("int abc, def;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 2);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK(def->type.get_type() == parser.type_int);
    CHECK_FALSE(def->type.is_const());
    CHECK_FALSE(def->type.is_volatile());

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration const applies to both variables") {
    SETUP("int const abc, def;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 2);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK(def->type.get_type() == parser.type_int);
    CHECK(def->type.is_const());
    CHECK_FALSE(def->type.is_volatile());

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration second variable is pointer") {
    SETUP("int abc, *def;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 2);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK_FALSE(def->type.is_const());
    CHECK_FALSE(def->type.is_volatile());
    Type* def_t = def->type.get_type();
    REQUIRE(def_t);
    REQUIRE(def_t->tag == Type::Pointer);
    Type_Pointer* def_type = (Type_Pointer*)def_t;
    CHECK(def_type->inner.get_type() == parser.type_int);
    CHECK_FALSE(def_type->inner.is_const());
    CHECK_FALSE(def_type->inner.is_volatile());

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration const cannot be used after an identifier") {
    SETUP("int abc, const *def;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::ErrorInvalidInput);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    CHECK(context.errors.len() == 1);
}

TEST_CASE("parse_declaration struct unnamed empty body") {
    SETUP("struct {};");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.type_stack.len() == 1);
    CHECK(parser.type_stack[0].count == 0);
    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration struct named empty body") {
    SETUP("struct S {};");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.type_stack.len() == 1);
    REQUIRE(parser.type_stack[0].count == 1);
    Type** type = parser.type_stack[0].get_hash("S");
    REQUIRE(type);
    REQUIRE(*type);
    REQUIRE((*type)->tag == Type::Struct);
    Type_Struct* ts = (Type_Struct*)*type;
    REQUIRE(ts->types.count == 0);
    REQUIRE(ts->typedefs.count == 0);
    REQUIRE(ts->declarations.count == 0);
    REQUIRE(ts->initializers.len == 0);
    REQUIRE(ts->flags == Type_Struct::Defined);

    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration struct named two fields") {
    SETUP("struct S { int x = 3; float y; };");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.type_stack.len() == 1);
    REQUIRE(parser.type_stack[0].count == 1);
    Type** type = parser.type_stack[0].get_hash("S");
    REQUIRE(type);
    REQUIRE(*type);
    REQUIRE((*type)->tag == Type::Struct);
    Type_Struct* ts = (Type_Struct*)*type;
    REQUIRE(ts->types.count == 0);
    REQUIRE(ts->typedefs.count == 0);
    REQUIRE(ts->declarations.count == 2);
    Declaration* x = ts->declarations.get_hash("x");
    REQUIRE(x);
    CHECK(x->type.get_type() == parser.type_int);
    CHECK_FALSE(x->type.is_const());
    CHECK_FALSE(x->type.is_volatile());
    Declaration* y = ts->declarations.get_hash("y");
    REQUIRE(y);
    CHECK(y->type.get_type() == parser.type_float);
    CHECK_FALSE(y->type.is_const());
    CHECK_FALSE(y->type.is_volatile());
    REQUIRE(ts->initializers.len == 2);
    REQUIRE(ts->initializers[0]);
    CHECK(ts->initializers[0]->tag == Statement::Initializer_Copy);
    REQUIRE(ts->initializers[1]);
    CHECK(ts->initializers[1]->tag == Statement::Initializer_Default);
    REQUIRE(ts->flags == Type_Struct::Defined);

    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration struct named with variable") {
    SETUP("struct S {} s;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.type_stack.len() == 1);
    REQUIRE(parser.type_stack[0].count == 1);
    Type** type = parser.type_stack[0].get_hash("S");
    REQUIRE(type);
    REQUIRE(*type);
    REQUIRE((*type)->tag == Type::Struct);
    Type_Struct* ts = (Type_Struct*)*type;
    REQUIRE(ts->types.count == 0);
    REQUIRE(ts->typedefs.count == 0);
    REQUIRE(ts->declarations.count == 0);
    REQUIRE(ts->initializers.len == 0);
    REQUIRE(ts->flags == Type_Struct::Defined);

    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    REQUIRE(initializers.len() == 1);
    REQUIRE(initializers[0]);
    REQUIRE(initializers[0]->tag == Statement::Initializer_Default);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration union unnamed empty body") {
    SETUP("union {};");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.type_stack.len() == 1);
    CHECK(parser.type_stack[0].count == 0);
    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration union named empty body") {
    SETUP("union S {};");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.type_stack.len() == 1);
    REQUIRE(parser.type_stack[0].count == 1);
    Type** type = parser.type_stack[0].get_hash("S");
    REQUIRE(type);
    REQUIRE(*type);
    REQUIRE((*type)->tag == Type::Union);
    Type_Union* ts = (Type_Union*)*type;
    REQUIRE(ts->types.count == 0);
    REQUIRE(ts->typedefs.count == 0);
    REQUIRE(ts->declarations.count == 0);
    REQUIRE(ts->flags == Type_Union::Defined);

    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration union named two fields") {
    SETUP("union S { int x = 3; float y; };");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.type_stack.len() == 1);
    REQUIRE(parser.type_stack[0].count == 1);
    Type** type = parser.type_stack[0].get_hash("S");
    REQUIRE(type);
    REQUIRE(*type);
    REQUIRE((*type)->tag == Type::Union);
    Type_Union* ts = (Type_Union*)*type;
    REQUIRE(ts->types.count == 0);
    REQUIRE(ts->typedefs.count == 0);
    REQUIRE(ts->declarations.count == 2);
    Declaration* x = ts->declarations.get_hash("x");
    REQUIRE(x);
    CHECK(x->type.get_type() == parser.type_int);
    CHECK_FALSE(x->type.is_const());
    CHECK_FALSE(x->type.is_volatile());
    Declaration* y = ts->declarations.get_hash("y");
    REQUIRE(y);
    CHECK(y->type.get_type() == parser.type_float);
    CHECK_FALSE(y->type.is_const());
    CHECK_FALSE(y->type.is_volatile());
    REQUIRE(ts->flags == Type_Union::Defined);

    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);

    CHECK(context.errors.len() == 1);
}

TEST_CASE("parse_declaration union named with variable") {
    SETUP("union S {} s;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.type_stack.len() == 1);
    REQUIRE(parser.type_stack[0].count == 1);
    Type** type = parser.type_stack[0].get_hash("S");
    REQUIRE(type);
    REQUIRE(*type);
    REQUIRE((*type)->tag == Type::Union);
    Type_Union* ts = (Type_Union*)*type;
    REQUIRE(ts->types.count == 0);
    REQUIRE(ts->typedefs.count == 0);
    REQUIRE(ts->declarations.count == 0);
    REQUIRE(ts->flags == Type_Union::Defined);

    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    REQUIRE(initializers.len() == 1);
    REQUIRE(initializers[0]);
    REQUIRE(initializers[0]->tag == Statement::Initializer_Default);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration struct declaration and then usage without tag is error") {
    SETUP("struct S {}; S s;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(initializers.len() == 1);
    REQUIRE(parser.declaration_stack[0].count == 1);
    Declaration* s = parser.declaration_stack[0].get_hash("s");
    REQUIRE(s);
    REQUIRE(parser.type_stack[0].count == 1);
    Type** ts = parser.type_stack[0].get_hash("S");
    REQUIRE(ts);
    CHECK(s->type.get_type() == *ts);

    CHECK(context.errors.len() == 1);
}

TEST_CASE("parse_declaration struct declaration and then usage with tag is ok") {
    SETUP("struct S {}; struct S s;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(initializers.len() == 1);
    REQUIRE(parser.declaration_stack[0].count == 1);
    Declaration* s = parser.declaration_stack[0].get_hash("s");
    REQUIRE(s);
    REQUIRE(parser.type_stack[0].count == 1);
    Type** ts = parser.type_stack[0].get_hash("S");
    REQUIRE(ts);
    CHECK(s->type.get_type() == *ts);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration typedef unnamed struct declaration and then usage without tag is ok") {
    SETUP("typedef struct {} S; S s;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);

    REQUIRE(parser.type_stack.len() == 1);
    REQUIRE(parser.type_stack[0].count == 0);
    REQUIRE(parser.typedef_stack.len() == 1);
    REQUIRE(parser.typedef_stack[0].count == 1);
    REQUIRE(parser.declaration_stack.len() == 1);
    REQUIRE(parser.declaration_stack[0].count == 1);
}

TEST_CASE("parse_declaration typedef named struct declaration and then usage without tag is ok") {
    SETUP("typedef struct S {} S; S s;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);

    REQUIRE(parser.type_stack.len() == 1);
    REQUIRE(parser.type_stack[0].count == 1);
    REQUIRE(parser.typedef_stack.len() == 1);
    REQUIRE(parser.typedef_stack[0].count == 1);
    REQUIRE(parser.declaration_stack.len() == 1);
    REQUIRE(parser.declaration_stack[0].count == 1);
}

TEST_CASE(
    "parse_declaration typedef unnamed struct declaration and then usage with tag is different "
    "type") {
    SETUP("typedef struct {} S; struct S s;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);

    REQUIRE(parser.type_stack.len() == 1);
    REQUIRE(parser.type_stack[0].count == 1);
    REQUIRE(parser.typedef_stack.len() == 1);
    REQUIRE(parser.typedef_stack[0].count == 1);
    REQUIRE(parser.declaration_stack.len() == 1);
    REQUIRE(parser.declaration_stack[0].count == 1);
    Declaration* s = parser.declaration_stack[0].get_hash("s");
    Type** type_s = parser.type_stack[0].get_hash("S");
    TypeP* typedef_s = parser.typedef_stack[0].get_hash("S");
    REQUIRE(s);
    REQUIRE(type_s);
    REQUIRE(typedef_s);
    CHECK(s->type.get_type() == *type_s);
    CHECK(typedef_s->get_type() != *type_s);
}

TEST_CASE("parse_declaration enum with no values does nothing") {
    SETUP("enum {};");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.type_stack.len() == 1);
    CHECK(parser.type_stack[0].count == 0);
    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);
    CHECK(initializers.len() == 0);
}

TEST_CASE("parse_declaration enum with two values") {
    SETUP("enum { A, B };");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.type_stack.len() == 1);
    CHECK(parser.type_stack[0].count == 0);
    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 2);
    CHECK(initializers.len() == 2);
}

TEST_CASE("parse_declaration unnamed struct no body") {
    SETUP("struct* a;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 1);
    REQUIRE(parser.type_stack.len() == 1);
    CHECK(parser.type_stack[0].count == 0);
    REQUIRE(parser.typedef_stack.len() == 1);
    CHECK(parser.typedef_stack[0].count == 0);
    REQUIRE(parser.declaration_stack.len() == 1);

    REQUIRE(parser.declaration_stack[0].count == 1);
    Declaration* a = parser.declaration_stack[0].get_hash("a");
    REQUIRE(a);
    REQUIRE(a->type.get_type());
    CHECK(a->type.get_type()->tag == Type::Pointer);
    Type_Pointer* ap = (Type_Pointer*)a->type.get_type();
    REQUIRE(ap->inner.get_type());
    CHECK(ap->inner.get_type()->tag == Type::Builtin_Error);

    CHECK(initializers.len() == 1);
}

TEST_CASE("parse_expression defined variable") {
    SETUP("int abc; abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Variable);
    Expression_Variable* e = (Expression_Variable*)expression;
    CHECK(e->variable.str == "abc");

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_expression undefined variable") {
    SETUP("abc;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::ErrorInvalidInput);

    CHECK(context.errors.len() == 1);
}

TEST_CASE("parse_expression integer") {
    SETUP("123;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Integer);
    Expression_Integer* e = (Expression_Integer*)expression;
    CHECK(e->value == 123);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_expression basic binary expression") {
    SETUP("1 + 2;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);
    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Plus);

    REQUIRE(e->left);
    REQUIRE(e->left->tag == Expression::Integer);
    Expression_Integer* left = (Expression_Integer*)e->left;
    CHECK(left->value == 1);

    REQUIRE(e->right);
    REQUIRE(e->right->tag == Expression::Integer);
    Expression_Integer* right = (Expression_Integer*)e->right;
    CHECK(right->value == 2);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_expression binary expression left to right") {
    SETUP("1 + 2 + 3;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);
    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Plus);

    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Binary);

    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Integer);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_expression binary expression right to left") {
    SETUP("1 = 2 = 3;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);
    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Set);

    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Integer);

    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Binary);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_expression binary expression left to right parenthesis") {
    SETUP("1 + (2 + 3);");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);
    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Plus);

    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Integer);

    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Binary);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_expression binary expression right to left parenthesis") {
    SETUP("(1 = 2) = 3;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);
    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Set);

    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Binary);

    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Integer);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_statement basic binary expression") {
    SETUP("1 + 2;");

    Statement* statement;
    REQUIRE(parse_statement(&context, &parser, &statement).type == Result::Success);
    REQUIRE(statement);
    REQUIRE(statement->tag == Statement::Expression);
    Expression* expression = ((Statement_Expression*)statement)->expression;

    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);
    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Plus);

    REQUIRE(e->left);
    REQUIRE(e->left->tag == Expression::Integer);
    Expression_Integer* left = (Expression_Integer*)e->left;
    CHECK(left->value == 1);

    REQUIRE(e->right);
    REQUIRE(e->right->tag == Expression::Integer);
    Expression_Integer* right = (Expression_Integer*)e->right;
    CHECK(right->value == 2);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration_or_statement basic binary expression") {
    SETUP("1 + 2;");

    cz::Vector<Statement*> statements = {};
    CZ_DEFER(statements.drop(cz::heap_allocator()));
    Declaration_Or_Statement which;
    REQUIRE(parse_declaration_or_statement(&context, &parser, &statements, &which).type ==
            Result::Success);
    REQUIRE(which == Declaration_Or_Statement::Statement);
    REQUIRE(statements.len() == 1);

    Statement* statement = statements[0];
    REQUIRE(statement);
    REQUIRE(statement->tag == Statement::Expression);
    Expression* expression = ((Statement_Expression*)statement)->expression;

    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);
    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Plus);

    REQUIRE(e->left);
    REQUIRE(e->left->tag == Expression::Integer);
    Expression_Integer* left = (Expression_Integer*)e->left;
    CHECK(left->value == 1);

    REQUIRE(e->right);
    REQUIRE(e->right->tag == Expression::Integer);
    Expression_Integer* right = (Expression_Integer*)e->right;
    CHECK(right->value == 2);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration_or_statement type with identifier") {
    SETUP("int abc;");
    cz::Vector<Statement*> statements = {};
    CZ_DEFER(statements.drop(cz::heap_allocator()));

    Declaration_Or_Statement which;
    REQUIRE(parse_declaration_or_statement(&context, &parser, &statements, &which).type ==
            Result::Success);
    REQUIRE(which == Declaration_Or_Statement::Declaration);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration two variables first has initializer") {
    SETUP("int abc = 13, def;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 2);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK(def->type.get_type() == parser.type_int);
    CHECK_FALSE(def->type.is_const());
    CHECK_FALSE(def->type.is_volatile());

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_statement block with defined variable and expression usage") {
    SETUP("{ int abc; abc; }");

    Statement* statement;
    REQUIRE(parse_statement(&context, &parser, &statement).type == Result::Success);
    REQUIRE(statement);
    REQUIRE(statement->tag == Statement::Block);

    Statement_Block* block = (Statement_Block*)statement;
    REQUIRE(block->statements.len == 2);

    REQUIRE(block->statements[0]);
    REQUIRE(block->statements[0]->tag == Statement::Initializer_Default);

    Statement* se = block->statements[1];
    REQUIRE(se);
    REQUIRE(se->tag == Statement::Expression);

    Expression* expression = ((Statement_Expression*)se)->expression;
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Variable);
    Expression_Variable* e = (Expression_Variable*)expression;
    CHECK(e->variable.str == "abc");

    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_statement for loop") {
    SETUP("int abc; for (abc = 0; abc < 5; abc = abc + 1) {}");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Statement* statement;
    REQUIRE(parse_statement(&context, &parser, &statement).type == Result::Success);
    REQUIRE(statement);
    REQUIRE(statement->tag == Statement::For);

    Statement_For* sfor = (Statement_For*)statement;
    REQUIRE(sfor->initializer);
    REQUIRE(sfor->initializer->tag == Expression::Binary);

    REQUIRE(sfor->condition);
    REQUIRE(sfor->condition->tag == Expression::Binary);

    REQUIRE(sfor->increment);
    REQUIRE(sfor->increment->tag == Expression::Binary);

    REQUIRE(sfor->body);
    REQUIRE(sfor->body->tag == Statement::Block);
    Statement_Block* block = (Statement_Block*)sfor->body;
    CHECK(block->statements.len == 0);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_statement while loop") {
    SETUP("while (0) {}");

    Statement* statement;
    REQUIRE(parse_statement(&context, &parser, &statement).type == Result::Success);
    REQUIRE(statement);
    REQUIRE(statement->tag == Statement::While);

    Statement_While* swhile = (Statement_While*)statement;
    REQUIRE(swhile->condition);
    REQUIRE(swhile->condition->tag == Expression::Integer);
    Expression_Integer* condition = (Expression_Integer*)swhile->condition;
    REQUIRE(condition->value == 0);

    REQUIRE(swhile->body);
    REQUIRE(swhile->body->tag == Statement::Block);
    Statement_Block* block = (Statement_Block*)swhile->body;
    CHECK(block->statements.len == 0);
}
