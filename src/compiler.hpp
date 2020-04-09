#pragma once

namespace red {
struct Context;
struct Result;

Result compile_file(Context*, const char* file_name);

}
