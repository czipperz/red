#pragma once

#include <errno.h>

namespace red {

struct Result {
    enum Type {
        Success,
        ErrorSystem,
        ErrorFile,
    } type;
    union ErrorValue {
        int system;
    } error;

    static constexpr Result ok() { return {Success, {}}; }
    static Result last_system_error() {
        Result result;
        result.type = ErrorSystem;
        result.error.system = errno;
        return result;
    }
    constexpr bool is_ok() const { return !is_err(); }
    constexpr bool is_err() const { return type != Success; }
};

}

namespace cz {

inline bool is_err(red::Result result) {
    return result.is_err();
}

}
