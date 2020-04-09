#pragma once

#include <cz/str.hpp>
#include "file_contents.hpp"

namespace red {

struct File {
    cz::Str path;
    File_Contents contents;
};

}
