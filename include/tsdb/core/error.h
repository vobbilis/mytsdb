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
    explicit Error(const std::string& message) : std::runtime_error(message) {}
    explicit Error(const char* message) : std::runtime_error(message) {}
};

/**
 * @brief Error indicating invalid arguments or parameters
 */
class InvalidArgumentError : public Error {
public:
    explicit InvalidArgumentError(const std::string& message) : Error(message) {}
    explicit InvalidArgumentError(const char* message) : Error(message) {}
};

/**
 * @brief Error indicating resource not found
 */
class NotFoundError : public Error {
public:
    explicit NotFoundError(const std::string& message) : Error(message) {}
    explicit NotFoundError(const char* message) : Error(message) {}
};

/**
 * @brief Error indicating resource already exists
 */
class AlreadyExistsError : public Error {
public:
    explicit AlreadyExistsError(const std::string& message) : Error(message) {}
    explicit AlreadyExistsError(const char* message) : Error(message) {}
};

/**
 * @brief Error indicating operation timeout
 */
class TimeoutError : public Error {
public:
    explicit TimeoutError(const std::string& message) : Error(message) {}
    explicit TimeoutError(const char* message) : Error(message) {}
};

/**
 * @brief Error indicating resource exhaustion
 */
class ResourceExhaustedError : public Error {
public:
    explicit ResourceExhaustedError(const std::string& message) : Error(message) {}
    explicit ResourceExhaustedError(const char* message) : Error(message) {}
};

/**
 * @brief Error indicating internal error
 */
class InternalError : public Error {
public:
    explicit InternalError(const std::string& message) : Error(message) {}
    explicit InternalError(const char* message) : Error(message) {}
};

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_ERROR_H_ 