#pragma once

#include <cz/string.hpp>
#include "context.hpp"
#include "preprocess.hpp"

namespace red {

Result load_file(C* c, cpp::Preprocessor* p, cz::String file_path);

}
