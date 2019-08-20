#pragma once

namespace red {

struct Result {
    enum Type {
        Success,
        Error,
    };
    Type type;

    static constexpr Result ok() { return Result{Success}; }
    constexpr bool is_ok() const { return !is_err(); }
    constexpr bool is_err() const { return type != Success; }
};

}

namespace cz {

inline bool is_err(red::Result result) {
    return result.is_err();
}

}
