#ifndef TSDB_CORE_RESULT_H_
#define TSDB_CORE_RESULT_H_

#include <string>
#include <variant>
#include <type_traits>
#include <memory>
#include <stdexcept>
#include <vector>
#include "tsdb/core/error.h"

namespace tsdb {
namespace core {

/**
 * @brief Result type for operations that can fail
 * 
 * Usage:
 * ```
 * Result<int> foo() {
 *     if (error_condition) {
 *         return Result<int>::error("error message");
 *     }
 *     return Result<int>(42);
 * }
 * 
 * auto result = foo();
 * if (result.ok()) {
 *     int value = result.value();
 * } else {
 *     std::string error = result.error();
 * }
 * ```
 */
template<typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)), error_(nullptr) {}
    Result(std::unique_ptr<Error> error) : error_(std::move(error)) {}
    
    bool ok() const { return error_ == nullptr; }
    const Error& error() const { return *error_; }
    const T& value() const { return value_; }
    T&& take_value() { return std::move(value_); }
    
    static Result<T> error(const std::string& message) {
        return Result<T>(std::make_unique<Error>(message));
    }
    
private:
    T value_;
    std::unique_ptr<Error> error_;
};

/**
 * @brief Specialization for void results
 */
template<>
class Result<void> {
public:
    Result() : error_(nullptr) {}
    Result(std::unique_ptr<Error> error) : error_(std::move(error)) {}
    
    bool ok() const { return error_ == nullptr; }
    const Error& error() const { return *error_; }
    
    static Result<void> error(const std::string& message) {
        return Result<void>(std::make_unique<Error>(message));
    }
    
private:
    std::unique_ptr<Error> error_;
};

// Common template instantiations declarations
extern template class Result<std::vector<uint8_t>>;
extern template class Result<std::string>;

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_RESULT_H_ 