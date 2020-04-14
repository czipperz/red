#include "definition.hpp"

#include <stdint.h>
#include <cz/bit_array.hpp>
#include <cz/string.hpp>
#include <cz/util.hpp>

namespace red {
namespace pre {

void Definition::drop(cz::Allocator allocator) {
    tokens.drop(allocator);
}

}
}
