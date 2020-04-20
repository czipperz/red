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
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    REQUIRE(initializers.len() == 1);
    REQUIRE(initializers[0]);
    REQUIRE(initializers[0]->tag == Statement::Initializer_Default);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_signed_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
    CHECK(abc->v.initializer == initializers[0]);

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Done);
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
    CHECK(abc->span.start.index == 4);
    CHECK(abc->span.end.index == 7);
    CHECK(abc->type.get_type() == parser.type_signed_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK(def->span.start.index == 9);
    CHECK(def->span.end.index == 12);
    CHECK(def->type.get_type() == parser.type_signed_int);
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
    CHECK(abc->type.get_type() == parser.type_signed_int);
    CHECK(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK(def->type.get_type() == parser.type_signed_int);
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
    CHECK(abc->type.get_type() == parser.type_signed_int);
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
    CHECK(def_type->inner.get_type() == parser.type_signed_int);
    CHECK_FALSE(def_type->inner.is_const());
    CHECK_FALSE(def_type->inner.is_volatile());

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration const cannot be used after an identifier") {
    SETUP("int abc, const *def;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 2);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_signed_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK_FALSE(def->type.is_const());
    CHECK_FALSE(def->type.is_volatile());
    REQUIRE(def->type.get_type());
    REQUIRE(def->type.get_type()->tag == Type::Pointer);
    Type_Pointer* deft = (Type_Pointer*)def->type.get_type();
    CHECK(deft->inner.get_type() == parser.type_signed_int);
    CHECK_FALSE(deft->inner.is_const());
    CHECK_FALSE(deft->inner.is_volatile());

    CHECK(context.errors.len() == 1);
}

TEST_CASE("parse_declaration static") {
    SETUP("static int abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    REQUIRE(initializers.len() == 1);
    REQUIRE(initializers[0]);
    REQUIRE(initializers[0]->tag == Statement::Initializer_Default);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_signed_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
    CHECK(abc->v.initializer == initializers[0]);
    CHECK(abc->flags == Declaration::Static);

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Done);
    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_declaration basic function declaration no parameters") {
    SETUP("void f();");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* f = parser.declaration_stack[0].get_hash("f");
    REQUIRE(f);
    CHECK_FALSE(f->type.is_const());
    CHECK_FALSE(f->type.is_volatile());
    REQUIRE(f->type.get_type());
    REQUIRE(f->type.get_type()->tag == Type::Function);

    Type_Function* ft = (Type_Function*)f->type.get_type();
    CHECK(ft->return_type.get_type() == parser.type_void);
    CHECK(ft->parameter_types.len == 0);
    CHECK_FALSE(ft->has_varargs);
}

TEST_CASE("parse_declaration extern function declaration one parameter") {
    SETUP("extern void f(int x);");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* f = parser.declaration_stack[0].get_hash("f");
    REQUIRE(f);
    CHECK_FALSE(f->type.is_const());
    CHECK_FALSE(f->type.is_volatile());
    REQUIRE(f->type.get_type());
    REQUIRE(f->type.get_type()->tag == Type::Function);
    CHECK(f->flags == Declaration::Extern);

    Type_Function* ft = (Type_Function*)f->type.get_type();
    CHECK(ft->return_type.get_type() == parser.type_void);
    REQUIRE(ft->parameter_types.len == 1);
    CHECK(ft->parameter_types[0].get_type() == parser.type_signed_int);
    CHECK_FALSE(ft->parameter_types[0].is_const());
    CHECK_FALSE(ft->parameter_types[0].is_volatile());
    CHECK_FALSE(ft->has_varargs);
}

TEST_CASE("parse_declaration parenthesized variable name") {
    SETUP("int (abc);");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_signed_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
}

TEST_CASE("parse_declaration parenthesized variable name pointer") {
    SETUP("int (*abc);");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
    REQUIRE(abc->type.get_type());
    REQUIRE(abc->type.get_type()->tag == Type::Pointer);
    Type_Pointer* abct = (Type_Pointer*)abc->type.get_type();
    CHECK(abct->inner.get_type() == parser.type_signed_int);
}

TEST_CASE("parse_declaration function pointer one parameter") {
    SETUP("void (*f)(int x);");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* f = parser.declaration_stack[0].get_hash("f");
    REQUIRE(f);
    CHECK(f->span.start.index == 5);
    CHECK(f->span.end.index == 16);
    CHECK_FALSE(f->type.is_const());
    CHECK_FALSE(f->type.is_volatile());
    REQUIRE(f->type.get_type());
    REQUIRE(f->type.get_type()->tag == Type::Pointer);

    Type_Pointer* ft = (Type_Pointer*)f->type.get_type();
    REQUIRE(ft);
    CHECK_FALSE(ft->inner.is_const());
    CHECK_FALSE(ft->inner.is_volatile());
    REQUIRE(ft->inner.get_type());
    REQUIRE(ft->inner.get_type()->tag == Type::Function);

    Type_Function* fun = (Type_Function*)ft->inner.get_type();
    CHECK(fun->return_type.get_type() == parser.type_void);
    REQUIRE(fun->parameter_types.len == 1);
    CHECK(fun->parameter_types[0].get_type() == parser.type_signed_int);
    CHECK_FALSE(fun->parameter_types[0].is_const());
    CHECK_FALSE(fun->parameter_types[0].is_volatile());
    CHECK_FALSE(fun->has_varargs);
}

TEST_CASE("parse_declaration function definition one parameter") {
    SETUP("void f(int x) {}");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* f = parser.declaration_stack[0].get_hash("f");
    REQUIRE(f);
    CHECK_FALSE(f->type.is_const());
    CHECK_FALSE(f->type.is_volatile());
    REQUIRE(f->type.get_type());
    REQUIRE(f->type.get_type()->tag == Type::Function);

    Type_Function* fun = (Type_Function*)f->type.get_type();
    CHECK(fun->return_type.get_type() == parser.type_void);
    REQUIRE(fun->parameter_types.len == 1);
    CHECK(fun->parameter_types[0].get_type() == parser.type_signed_int);
    CHECK_FALSE(fun->parameter_types[0].is_const());
    CHECK_FALSE(fun->parameter_types[0].is_volatile());
    CHECK_FALSE(fun->has_varargs);

    REQUIRE(f->v.function_definition);
    REQUIRE(f->v.function_definition->parameter_names.len == 1);
    CHECK(f->v.function_definition->parameter_names[0] == "x");
    REQUIRE(f->v.function_definition->block.statements.len == 0);
}

TEST_CASE("parse_declaration function definition one parameter and its used") {
    SETUP("void f(int x) { x; }");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* f = parser.declaration_stack[0].get_hash("f");
    REQUIRE(f);
    CHECK(f->span.start.index == 5);
    CHECK(f->span.end.index == 13);
    REQUIRE(f->type.get_type());
    REQUIRE(f->type.get_type()->tag == Type::Function);

    Type_Function* fun = (Type_Function*)f->type.get_type();
    CHECK(fun->return_type.get_type() == parser.type_void);
    REQUIRE(fun->parameter_types.len == 1);
    CHECK(fun->parameter_types[0].get_type() == parser.type_signed_int);
    CHECK_FALSE(fun->parameter_types[0].is_const());
    CHECK_FALSE(fun->parameter_types[0].is_volatile());
    CHECK_FALSE(fun->has_varargs);

    REQUIRE(f->v.function_definition);
    REQUIRE(f->v.function_definition->parameter_names.len == 1);
    CHECK(f->v.function_definition->parameter_names[0] == "x");

    REQUIRE(f->v.function_definition->block.statements.len == 1);
    Statement* statement = f->v.function_definition->block.statements[0];
    REQUIRE(statement);
    CHECK(statement->tag == Statement::Expression);
}

TEST_CASE("parse_declaration function returning function pointer one parameter") {
    SETUP("void (*f())(int x);");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* decl = parser.declaration_stack[0].get_hash("f");
    REQUIRE(decl);
    CHECK_FALSE(decl->type.is_const());
    CHECK_FALSE(decl->type.is_volatile());
    REQUIRE(decl->type.get_type());
    REQUIRE(decl->type.get_type()->tag == Type::Function);

    Type_Function* fun1 = (Type_Function*)decl->type.get_type();
    CHECK(fun1->parameter_types.len == 0);
    CHECK_FALSE(fun1->has_varargs);

    Type_Pointer* ret = (Type_Pointer*)fun1->return_type.get_type();
    REQUIRE(ret);
    CHECK_FALSE(ret->inner.is_const());
    CHECK_FALSE(ret->inner.is_volatile());
    REQUIRE(ret->inner.get_type());
    REQUIRE(ret->inner.get_type()->tag == Type::Function);

    Type_Function* fun = (Type_Function*)ret->inner.get_type();
    CHECK(fun->return_type.get_type() == parser.type_void);
    REQUIRE(fun->parameter_types.len == 1);
    CHECK(fun->parameter_types[0].get_type() == parser.type_signed_int);
    CHECK_FALSE(fun->parameter_types[0].is_const());
    CHECK_FALSE(fun->parameter_types[0].is_volatile());
    CHECK_FALSE(fun->has_varargs);
}

TEST_CASE("parse_declaration array of function pointer one parameter") {
    SETUP("void (*f[3])(int x);");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* decl = parser.declaration_stack[0].get_hash("f");
    REQUIRE(decl);
    CHECK_FALSE(decl->type.is_const());
    CHECK_FALSE(decl->type.is_volatile());
    REQUIRE(decl->type.get_type());
    REQUIRE(decl->type.get_type()->tag == Type::Array);

    Type_Array* array = (Type_Array*)decl->type.get_type();
    REQUIRE(array->o_length);
    CHECK(array->o_length->tag == Expression::Integer);
    REQUIRE(array->inner.get_type());
    REQUIRE(array->inner.get_type()->tag == Type::Pointer);

    Type_Pointer* ret = (Type_Pointer*)array->inner.get_type();
    REQUIRE(ret);
    CHECK_FALSE(ret->inner.is_const());
    CHECK_FALSE(ret->inner.is_volatile());
    REQUIRE(ret->inner.get_type());
    REQUIRE(ret->inner.get_type()->tag == Type::Function);

    Type_Function* fun = (Type_Function*)ret->inner.get_type();
    CHECK(fun->return_type.get_type() == parser.type_void);
    REQUIRE(fun->parameter_types.len == 1);
    CHECK(fun->parameter_types[0].get_type() == parser.type_signed_int);
    CHECK_FALSE(fun->parameter_types[0].is_const());
    CHECK_FALSE(fun->parameter_types[0].is_volatile());
    CHECK_FALSE(fun->has_varargs);
}

TEST_CASE("parse_declaration array no length") {
    SETUP("void* f[];");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* decl = parser.declaration_stack[0].get_hash("f");
    REQUIRE(decl);
    CHECK_FALSE(decl->type.is_const());
    CHECK_FALSE(decl->type.is_volatile());
    REQUIRE(decl->type.get_type());
    REQUIRE(decl->type.get_type()->tag == Type::Array);

    Type_Array* array = (Type_Array*)decl->type.get_type();
    CHECK_FALSE(array->o_length);
    REQUIRE(array->inner.get_type());
    REQUIRE(array->inner.get_type()->tag == Type::Pointer);

    Type_Pointer* ret = (Type_Pointer*)array->inner.get_type();
    REQUIRE(ret);
    CHECK_FALSE(ret->inner.is_const());
    CHECK_FALSE(ret->inner.is_volatile());
    CHECK(ret->inner.get_type() == parser.type_void);
}

TEST_CASE("parse_declaration long int = signed long") {
    SETUP("long int abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_signed_long);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
}

TEST_CASE("parse_declaration long unsigned int = unsigned long") {
    SETUP("long unsigned int abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_unsigned_long);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
}

TEST_CASE("parse_declaration volatile signed const = CV (signed)") {
    SETUP("volatile signed const abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_signed_int);
    CHECK(abc->type.is_const());
    CHECK(abc->type.is_volatile());
}

TEST_CASE("parse_declaration long double = long double") {
    SETUP("long double abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_long_double);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
}

TEST_CASE("parse_declaration signed = signed int") {
    SETUP("signed abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_signed_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
}

TEST_CASE("parse_declaration signed = unsigned int") {
    SETUP("unsigned abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_unsigned_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
}

TEST_CASE("parse_declaration char = char") {
    SETUP("char abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_char);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
}

TEST_CASE("parse_declaration char signed = char") {
    SETUP("char signed abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_signed_char);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
}

TEST_CASE("parse_declaration unsigned char = char") {
    SETUP("unsigned char abc;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    CHECK(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_unsigned_char);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
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
    CHECK(ts->span.start.index == 0);
    CHECK(ts->span.end.index == 11);
    CHECK(ts->types.count == 0);
    CHECK(ts->typedefs.count == 0);
    CHECK(ts->declarations.count == 0);
    CHECK(ts->initializers.len == 0);
    CHECK(ts->flags == Type_Struct::Defined);
    CHECK(ts->size == 0);
    CHECK(ts->alignment == 1);

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
    CHECK(ts->types.count == 0);
    CHECK(ts->typedefs.count == 0);
    CHECK(ts->size == 8);
    CHECK(ts->alignment == 4);
    REQUIRE(ts->declarations.count == 2);
    Declaration* x = ts->declarations.get_hash("x");
    REQUIRE(x);
    CHECK(x->type.get_type() == parser.type_signed_int);
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
    CHECK(x->v.initializer == ts->initializers[0]);
    CHECK(y->v.initializer == ts->initializers[1]);

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
    CHECK(ts->types.count == 0);
    CHECK(ts->typedefs.count == 0);
    CHECK(ts->declarations.count == 0);
    CHECK(ts->flags == Type_Union::Defined);

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
    CHECK(ts->types.count == 0);
    CHECK(ts->typedefs.count == 0);
    CHECK(ts->size == 4);
    CHECK(ts->alignment == 4);
    CHECK(ts->flags == Type_Union::Defined);

    REQUIRE(ts->declarations.count == 2);
    Declaration* x = ts->declarations.get_hash("x");
    REQUIRE(x);
    CHECK(x->type.get_type() == parser.type_signed_int);
    CHECK_FALSE(x->type.is_const());
    CHECK_FALSE(x->type.is_volatile());
    Declaration* y = ts->declarations.get_hash("y");
    REQUIRE(y);
    CHECK(y->type.get_type() == parser.type_float);
    CHECK_FALSE(y->type.is_const());
    CHECK_FALSE(y->type.is_volatile());

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
    CHECK(ts->types.count == 0);
    CHECK(ts->typedefs.count == 0);
    CHECK(ts->declarations.count == 0);
    CHECK(ts->flags == Type_Union::Defined);

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
    Type_Definition* typedef_s = parser.typedef_stack[0].get_hash("S");
    REQUIRE(s);
    REQUIRE(type_s);
    REQUIRE(typedef_s);
    CHECK(s->type.get_type() == *type_s);
    CHECK(typedef_s->type.get_type() != *type_s);
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
    CHECK(initializers.len() == 0);
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
    CHECK(e->span.start.index == 9);
    CHECK(e->span.end.index == 12);

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
    CHECK(e->span.start.index == 0);
    CHECK(e->span.end.index == 11);

    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Integer);
    CHECK(e->left->span.start.index == 0);
    CHECK(e->left->span.end.index == 1);

    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Binary);
    CHECK(e->right->span.start.index == 4);
    CHECK(e->right->span.end.index == 11);

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

TEST_CASE("parse_expression ternary operator") {
    SETUP("1 ? 2 : 3;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Ternary);
    Expression_Ternary* e = (Expression_Ternary*)expression;

    REQUIRE(e->condition);
    CHECK(e->condition->tag == Expression::Integer);

    REQUIRE(e->then);
    CHECK(e->then->tag == Expression::Integer);

    REQUIRE(e->otherwise);
    CHECK(e->otherwise->tag == Expression::Integer);
}

TEST_CASE("parse_expression ternary inside ternary") {
    SETUP("1 ? 2 ? 3 : 4 : 5 ? 6 : 7;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Ternary);
    Expression_Ternary* e = (Expression_Ternary*)expression;

    REQUIRE(e->condition);
    CHECK(e->condition->tag == Expression::Integer);

    REQUIRE(e->then);
    CHECK(e->then->tag == Expression::Ternary);
    Expression_Ternary* then = (Expression_Ternary*)e->then;
    REQUIRE(then->condition);
    CHECK(then->condition->tag == Expression::Integer);
    REQUIRE(then->then);
    CHECK(then->then->tag == Expression::Integer);
    REQUIRE(then->otherwise);
    CHECK(then->otherwise->tag == Expression::Integer);

    REQUIRE(e->otherwise);
    CHECK(e->otherwise->tag == Expression::Ternary);
    Expression_Ternary* otherwise = (Expression_Ternary*)e->otherwise;
    REQUIRE(otherwise->condition);
    CHECK(otherwise->condition->tag == Expression::Integer);
    REQUIRE(otherwise->then);
    CHECK(otherwise->then->tag == Expression::Integer);
    REQUIRE(otherwise->otherwise);
    CHECK(otherwise->otherwise->tag == Expression::Integer);
}

TEST_CASE("parse_expression ternary operator comma in middle") {
    SETUP("1 ? 2, 3 : 4;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Ternary);
    Expression_Ternary* e = (Expression_Ternary*)expression;

    REQUIRE(e->condition);
    CHECK(e->condition->tag == Expression::Integer);

    REQUIRE(e->then);
    CHECK(e->then->tag == Expression::Binary);

    REQUIRE(e->otherwise);
    CHECK(e->otherwise->tag == Expression::Integer);
}

TEST_CASE("parse_expression ternary operator comma breaks otherwise") {
    SETUP("1 ? 2 : 3, 4;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);

    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->span.start.index == 0);
    CHECK(e->span.end.index == 12);
    CHECK(e->op == Token::Comma);
    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Ternary);
    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Integer);

    Expression_Ternary* ternary = (Expression_Ternary*)e->left;
    CHECK(ternary->span.start.index == 0);
    CHECK(ternary->span.end.index == 9);
    REQUIRE(ternary->condition);
    CHECK(ternary->condition->tag == Expression::Integer);

    REQUIRE(ternary->then);
    CHECK(ternary->then->tag == Expression::Integer);

    REQUIRE(ternary->otherwise);
    CHECK(ternary->otherwise->tag == Expression::Integer);
}

TEST_CASE("parse_expression type cast") {
    SETUP("(int)2 + 3;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);

    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Plus);
    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Cast);
    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Integer);

    Expression_Cast* cast = (Expression_Cast*)e->left;
    REQUIRE(cast->value);
    CHECK(cast->value->tag == Expression::Integer);
    CHECK(cast->type.get_type() == parser.type_signed_int);
}

TEST_CASE("parse_expression sizeof parenthesized type") {
    SETUP("sizeof(int);");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Sizeof_Type);

    Expression_Sizeof_Type* st = (Expression_Sizeof_Type*)expression;
    CHECK(st->type.get_type() == parser.type_signed_int);
}

TEST_CASE("parse_expression sizeof parenthesized expression") {
    SETUP("sizeof(1 + 2);");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Sizeof_Expression);

    Expression_Sizeof_Expression* se = (Expression_Sizeof_Expression*)expression;
    REQUIRE(se->expression->tag == Expression::Binary);

    Expression_Binary* e = (Expression_Binary*)se->expression;
    CHECK(e->op == Token::Plus);
    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Integer);
    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Integer);
}

TEST_CASE("parse_expression sizeof unparenthesized expression") {
    SETUP("sizeof 1 + 2;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);

    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Plus);
    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Sizeof_Expression);
    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Integer);

    Expression_Sizeof_Expression* se = (Expression_Sizeof_Expression*)e->left;
    CHECK(se->expression->tag == Expression::Integer);
}

TEST_CASE("parse_expression function call one argument") {
    SETUP("void f(int); int abc; f(abc);");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Function_Call);

    Expression_Function_Call* se = (Expression_Function_Call*)expression;
    CHECK(se->span.start.index == 22);
    CHECK(se->span.end.index == 28);
    REQUIRE(se->function);
    REQUIRE(se->function->tag == Expression::Variable);
    Expression_Variable* fun = (Expression_Variable*)se->function;
    CHECK(fun->variable.str == "f");

    REQUIRE(se->arguments.len == 1);
    REQUIRE(se->arguments[0]);
    REQUIRE(se->arguments[0]->tag == Expression::Variable);
    Expression_Variable* arg0 = (Expression_Variable*)se->arguments[0];
    CHECK(arg0->variable.str == "abc");
}

TEST_CASE("parse_expression function call no arguments") {
    SETUP("void f(); int abc; f();");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Function_Call);

    Expression_Function_Call* se = (Expression_Function_Call*)expression;
    CHECK(se->span.start.index == 19);
    CHECK(se->span.end.index == 22);
    REQUIRE(se->function);
    REQUIRE(se->function->tag == Expression::Variable);
    Expression_Variable* fun = (Expression_Variable*)se->function;
    CHECK(fun->variable.str == "f");

    CHECK(se->arguments.len == 0);
}

TEST_CASE("parse_expression function call multiple arguments") {
    SETUP("void f(); int abc; f(abc + 2, 3);");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Function_Call);

    Expression_Function_Call* se = (Expression_Function_Call*)expression;
    CHECK(se->span.start.index == 19);
    CHECK(se->span.end.index == 32);
    REQUIRE(se->function);
    REQUIRE(se->function->tag == Expression::Variable);
    Expression_Variable* fun = (Expression_Variable*)se->function;
    CHECK(fun->variable.str == "f");

    REQUIRE(se->arguments.len == 2);
    REQUIRE(se->arguments[0]);
    CHECK(se->arguments[0]->tag == Expression::Binary);
    REQUIRE(se->arguments[1]);
    CHECK(se->arguments[1]->tag == Expression::Integer);
}

TEST_CASE("parse_expression function call repeated calls") {
    SETUP("void f(int, int); int abc; f(abc + 2, 3)() + 1;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);

    Expression_Binary* be = (Expression_Binary*)expression;
    REQUIRE(be->left);
    REQUIRE(be->left->tag == Expression::Function_Call);
    REQUIRE(be->right);
    REQUIRE(be->right->tag == Expression::Integer);

    Expression_Function_Call* fne = (Expression_Function_Call*)be->left;
    REQUIRE(fne->function);
    REQUIRE(fne->function->tag == Expression::Function_Call);
    CHECK(fne->arguments.len == 0);

    Expression_Function_Call* se = (Expression_Function_Call*)fne->function;
    CHECK(se->span.start.index == 27);
    CHECK(se->span.end.index == 40);
    REQUIRE(se->function);
    REQUIRE(se->function->tag == Expression::Variable);
    Expression_Variable* fun = (Expression_Variable*)se->function;
    CHECK(fun->variable.str == "f");

    REQUIRE(se->arguments.len == 2);
    REQUIRE(se->arguments[0]);
    CHECK(se->arguments[0]->tag == Expression::Binary);
    REQUIRE(se->arguments[1]);
    CHECK(se->arguments[1]->tag == Expression::Integer);
}

TEST_CASE("parse_expression function call order of operations") {
    SETUP("void f(int, int); int g(); int abc; f(abc + 2, 3) + g();");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);
    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);

    Expression_Binary* be = (Expression_Binary*)expression;
    REQUIRE(be->left);
    REQUIRE(be->left->tag == Expression::Function_Call);
    REQUIRE(be->right);
    REQUIRE(be->right->tag == Expression::Function_Call);

    Expression_Function_Call* gne = (Expression_Function_Call*)be->right;
    CHECK(gne->span.start.index == 52);
    CHECK(gne->span.end.index == 55);
    REQUIRE(gne->function);
    REQUIRE(gne->function->tag == Expression::Variable);
    CHECK(gne->arguments.len == 0);

    Expression_Function_Call* se = (Expression_Function_Call*)be->left;
    CHECK(se->span.start.index == 36);
    CHECK(se->span.end.index == 49);
    REQUIRE(se->function);
    REQUIRE(se->function->tag == Expression::Variable);
    Expression_Variable* fun = (Expression_Variable*)se->function;
    CHECK(fun->variable.str == "f");

    REQUIRE(se->arguments.len == 2);
    REQUIRE(se->arguments[0]);
    CHECK(se->arguments[0]->tag == Expression::Binary);
    REQUIRE(se->arguments[1]);
    CHECK(se->arguments[1]->tag == Expression::Integer);
}

TEST_CASE("parse_expression index") {
    SETUP("int a[3]; a[0];");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Index);

    Expression_Index* ie = (Expression_Index*)expression;
    REQUIRE(ie->array);
    REQUIRE(ie->array->tag == Expression::Variable);
    REQUIRE(ie->index);
    REQUIRE(ie->index->tag == Expression::Integer);

    Expression_Variable* arr = (Expression_Variable*)ie->array;
    CHECK(arr->variable.str == "a");

    Expression_Integer* integer = (Expression_Integer*)ie->index;
    CHECK(integer->value == 0);
}

TEST_CASE("parse_expression address of array index") {
    SETUP("int a[3]; &a[0];");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Address_Of);

    Expression_Address_Of* ae = (Expression_Address_Of*)expression;
    REQUIRE(ae->value);
    REQUIRE(ae->value->tag == Expression::Index);

    Expression_Index* ie = (Expression_Index*)ae->value;
    REQUIRE(ie->array);
    REQUIRE(ie->array->tag == Expression::Variable);
    REQUIRE(ie->index);
    REQUIRE(ie->index->tag == Expression::Integer);

    Expression_Variable* arr = (Expression_Variable*)ie->array;
    CHECK(arr->variable.str == "a");

    Expression_Integer* integer = (Expression_Integer*)ie->index;
    CHECK(integer->value == 0);
}

TEST_CASE("parse_expression dereference value") {
    SETUP("int a[3]; *a + 2;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);

    Expression_Binary* be = (Expression_Binary*)expression;
    REQUIRE(be->left);
    REQUIRE(be->left->tag == Expression::Dereference);
    REQUIRE(be->right);
    REQUIRE(be->right->tag == Expression::Integer);

    Expression_Dereference* ae = (Expression_Dereference*)be->left;
    REQUIRE(ae->value);
    REQUIRE(ae->value->tag == Expression::Variable);

    Expression_Variable* arr = (Expression_Variable*)ae->value;
    CHECK(arr->variable.str == "a");

    Expression_Integer* inte = (Expression_Integer*)be->right;
    CHECK(inte->value == 2);
}

TEST_CASE("parse_expression bitwise not") {
    SETUP("~2;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Bit_Not);

    Expression_Bit_Not* bne = (Expression_Bit_Not*)expression;
    REQUIRE(bne->value);
    REQUIRE(bne->value->tag == Expression::Integer);

    Expression_Integer* inte = (Expression_Integer*)bne->value;
    CHECK(inte->value == 2);
}

TEST_CASE("parse_expression logical not") {
    SETUP("!2;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Logical_Not);

    Expression_Logical_Not* lne = (Expression_Logical_Not*)expression;
    REQUIRE(lne->value);
    REQUIRE(lne->value->tag == Expression::Integer);

    Expression_Integer* inte = (Expression_Integer*)lne->value;
    CHECK(inte->value == 2);
}

TEST_CASE("parse_expression dot operator") {
    SETUP("struct { int b; } a; a.b;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Member_Access);

    Expression_Member_Access* mae = (Expression_Member_Access*)expression;
    REQUIRE(mae->object);
    REQUIRE(mae->object->tag == Expression::Variable);
    Expression_Variable* obj = (Expression_Variable*)mae->object;
    CHECK(obj->variable.str == "a");
    REQUIRE(mae->field.str == "b");
}

TEST_CASE("parse_expression arrow operator") {
    SETUP("struct { int b; }* a; a->b;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Dereference_Member_Access);

    Expression_Dereference_Member_Access* mae = (Expression_Dereference_Member_Access*)expression;
    REQUIRE(mae->pointer);
    REQUIRE(mae->pointer->tag == Expression::Variable);
    Expression_Variable* ptr = (Expression_Variable*)mae->pointer;
    CHECK(ptr->variable.str == "a");
    REQUIRE(mae->field.str == "b");
}

TEST_CASE("parse_expression type cast user defined typedef") {
    SETUP("typedef int A; (A)2 + 3;");
    cz::Vector<Statement*> initializers = {};
    CZ_DEFER(initializers.drop(cz::heap_allocator()));

    REQUIRE(parse_declaration(&context, &parser, &initializers).type == Result::Success);

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Binary);

    Expression_Binary* e = (Expression_Binary*)expression;
    CHECK(e->op == Token::Plus);
    REQUIRE(e->left);
    CHECK(e->left->tag == Expression::Cast);
    REQUIRE(e->right);
    CHECK(e->right->tag == Expression::Integer);

    Expression_Cast* cast = (Expression_Cast*)e->left;
    REQUIRE(cast->value);
    CHECK(cast->value->tag == Expression::Integer);
    CHECK(cast->type.get_type() == parser.type_signed_int);
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
    CHECK(abc->type.get_type() == parser.type_signed_int);
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
    CHECK(abc->span.start.index == 4);
    CHECK(abc->span.end.index == 7);
    CHECK(abc->type.get_type() == parser.type_signed_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());

    CHECK(abc->v.initializer == initializers[0]);
    REQUIRE(initializers[0]);
    CHECK(initializers[0]->span.start.index == 4);
    CHECK(initializers[0]->span.end.index == 12);

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK(def->span.start.index == 14);
    CHECK(def->span.end.index == 17);
    CHECK(def->type.get_type() == parser.type_signed_int);
    CHECK_FALSE(def->type.is_const());
    CHECK_FALSE(def->type.is_volatile());

    CHECK(def->v.initializer == initializers[1]);
    REQUIRE(initializers[1]);
    CHECK(initializers[1]->span.start.index == 14);
    CHECK(initializers[1]->span.end.index == 17);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_statement block with defined variable and expression usage") {
    SETUP("{ int abc; abc; }");

    Statement* statement;
    REQUIRE(parse_statement(&context, &parser, &statement).type == Result::Success);
    REQUIRE(statement);
    REQUIRE(statement->tag == Statement::Block);

    Statement_Block* body = (Statement_Block*)statement;
    CHECK(body->span.start.index == 0);
    CHECK(body->span.end.index == 17);
    REQUIRE(body->block.statements.len == 2);

    REQUIRE(body->block.statements[0]);
    REQUIRE(body->block.statements[0]->tag == Statement::Initializer_Default);

    Statement* se = body->block.statements[1];
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
    CHECK(sfor->span.start.index == 9);
    CHECK(sfor->span.end.index == 49);
    REQUIRE(sfor->initializer);
    REQUIRE(sfor->initializer->tag == Expression::Binary);

    REQUIRE(sfor->condition);
    REQUIRE(sfor->condition->tag == Expression::Binary);

    REQUIRE(sfor->increment);
    REQUIRE(sfor->increment->tag == Expression::Binary);

    REQUIRE(sfor->body);
    REQUIRE(sfor->body->tag == Statement::Block);
    Statement_Block* body = (Statement_Block*)sfor->body;
    CHECK(body->block.statements.len == 0);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("parse_statement while loop") {
    SETUP("while (0) {}");

    Statement* statement;
    REQUIRE(parse_statement(&context, &parser, &statement).type == Result::Success);
    REQUIRE(statement);
    REQUIRE(statement->tag == Statement::While);

    Statement_While* swhile = (Statement_While*)statement;
    CHECK(swhile->span.start.index == 0);
    CHECK(swhile->span.end.index == 12);
    REQUIRE(swhile->condition);
    REQUIRE(swhile->condition->tag == Expression::Integer);
    Expression_Integer* condition = (Expression_Integer*)swhile->condition;
    REQUIRE(condition->value == 0);

    REQUIRE(swhile->body);
    REQUIRE(swhile->body->tag == Statement::Block);
    Statement_Block* body = (Statement_Block*)swhile->body;
    CHECK(body->block.statements.len == 0);
}

TEST_CASE("parse_statement return no expression") {
    SETUP("return;");

    Statement* statement;
    REQUIRE(parse_statement(&context, &parser, &statement).type == Result::Success);
    REQUIRE(statement);
    REQUIRE(statement->tag == Statement::Return);

    Statement_Return* sreturn = (Statement_Return*)statement;
    CHECK(sreturn->span.start.index == 0);
    CHECK(sreturn->span.end.index == 7);
    CHECK_FALSE(sreturn->o_value);
}

TEST_CASE("parse_statement return expression") {
    SETUP("return 13;");

    Statement* statement;
    REQUIRE(parse_statement(&context, &parser, &statement).type == Result::Success);
    REQUIRE(statement);
    REQUIRE(statement->tag == Statement::Return);

    Statement_Return* sreturn = (Statement_Return*)statement;
    CHECK(sreturn->span.start.index == 0);
    CHECK(sreturn->span.end.index == 10);
    REQUIRE(sreturn->o_value);
    CHECK(sreturn->o_value->tag == Expression::Integer);
}
