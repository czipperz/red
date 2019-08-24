#pragma once

#include <errno.h>

namespace red {

struct Result {
    enum Type {
        Success = 0,
        Done = 1,
        ErrorSystem = -1,
        ErrorFile = -2,
        ErrorEndOfFile = -3,
        ErrorInvalidInput = -4,
    } type;
    union ErrorValue {
        int system;
    } error;

    static constexpr Result ok() { return {Success, {}}; }
    static constexpr Result done() { return {Done, {}}; }
    static Result last_system_error() {
        Result result;
        result.type = ErrorSystem;
        result.error.system = errno;
        return result;
    }
    constexpr bool is_ok() const { return !is_err(); }
    constexpr bool is_err() const { return type < 0; }
};

}

namespace cz {

inline bool is_err(red::Result result) {
    return result.is_err();
}

}
