#include "context.hpp"

namespace red {

void Context::destroy() {
    options.destroy(this);

    for (size_t i = 0; i < errors.len(); ++i) {
        errors[i].message.drop(allocator);
    }
    errors.drop(allocator);

    // @TODO: Change second allocator here (name allocator) when we change file
    // names to use multi arena allocator.
    files.destroy(allocator);
}

}
