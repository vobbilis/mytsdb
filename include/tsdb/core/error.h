#ifndef TSDB_CORE_ERROR_H_
#define TSDB_CORE_ERROR_H_

#include <stdexcept>
#include <string>

namespace tsdb {
namespace core {

/**
 * @brief Base class for all TSDB errors
 */
class Error : public std::runtime_error {
public:
    enum class Code {
        UNKNOWN = 0,
        INVALID_ARGUMENT = 1,
        NOT_FOUND = 2,
        ALREADY_EXISTS = 3,
        TIMEOUT = 4,
        RESOURCE_EXHAUSTED = 5,
        INTERNAL = 6
    };

    explicit Error(const std::string& message, Code code = Code::UNKNOWN) 
        : std::runtime_error(message), code_(code) {}
    explicit Error(const char* message, Code code = Code::UNKNOWN) 
        : std::runtime_error(message), code_(code) {}

    Code code() const { return code_; }
    const char* what() const noexcept override { return std::runtime_error::what(); }

private:
    Code code_;
};

/**
 * @brief Error indicating invalid arguments or parameters
 */
class InvalidArgumentError : public Error {
public:
    explicit InvalidArgumentError(const std::string& message) 
        : Error(message, Code::INVALID_ARGUMENT) {}
    explicit InvalidArgumentError(const char* message) 
        : Error(message, Code::INVALID_ARGUMENT) {}
};

/**
 * @brief Error indicating resource not found
 */
class NotFoundError : public Error {
public:
    explicit NotFoundError(const std::string& message) 
        : Error(message, Code::NOT_FOUND) {}
    explicit NotFoundError(const char* message) 
        : Error(message, Code::NOT_FOUND) {}
};

/**
 * @brief Error indicating resource already exists
 */
class AlreadyExistsError : public Error {
public:
    explicit AlreadyExistsError(const std::string& message) 
        : Error(message, Code::ALREADY_EXISTS) {}
    explicit AlreadyExistsError(const char* message) 
        : Error(message, Code::ALREADY_EXISTS) {}
};

/**
 * @brief Error indicating operation timeout
 */
class TimeoutError : public Error {
public:
    explicit TimeoutError(const std::string& message) 
        : Error(message, Code::TIMEOUT) {}
    explicit TimeoutError(const char* message) 
        : Error(message, Code::TIMEOUT) {}
};

/**
 * @brief Error indicating resource exhaustion
 */
class ResourceExhaustedError : public Error {
public:
    explicit ResourceExhaustedError(const std::string& message) 
        : Error(message, Code::RESOURCE_EXHAUSTED) {}
    explicit ResourceExhaustedError(const char* message) 
        : Error(message, Code::RESOURCE_EXHAUSTED) {}
};

/**
 * @brief Error indicating internal error
 */
class InternalError : public Error {
public:
    explicit InternalError(const std::string& message) 
        : Error(message, Code::INTERNAL) {}
    explicit InternalError(const char* message) 
        : Error(message, Code::INTERNAL) {}
};

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_ERROR_H_ 