#pragma once

#include <variant>
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include "tsdb/prometheus/model/types.h"

namespace tsdb {
namespace prometheus {
namespace promql {

// Forward declarations for struct types only
struct Scalar;
struct String;

/**
 * @brief Represents a scalar value (timestamp + float value)
 */
struct Scalar {
    int64_t timestamp;
    double value;

    bool operator==(const Scalar& other) const {
        return timestamp == other.timestamp && value == other.value;
    }
};

/**
 * @brief Represents a single sample in an instant vector (labels + timestamp + value)
 */
struct Sample {
    LabelSet metric;
    int64_t timestamp;
    double value;

    bool operator==(const Sample& other) const {
        return metric == other.metric && timestamp == other.timestamp && value == other.value;
    }
};

/**
 * @brief Represents an instant vector (a list of samples)
 */
using Vector = std::vector<Sample>;

/**
 * @brief Represents a single time series in a range vector (labels + list of points)
 */
struct Series {
    LabelSet metric;
    std::vector<tsdb::prometheus::Sample> samples; // Reusing model::Sample (t, v)

    bool operator==(const Series& other) const {
        return metric == other.metric && samples == other.samples;
    }
};

/**
 * @brief Represents a range vector (a list of series)
 */
using Matrix = std::vector<Series>;

/**
 * @brief Represents a string value
 */
struct String {
    int64_t timestamp;
    std::string value;

    bool operator==(const String& other) const {
        return timestamp == other.timestamp && value == other.value;
    }
};

/**
 * @brief Type of the value
 */
enum class ValueType {
    NONE,
    SCALAR,
    VECTOR,
    MATRIX,
    STRING
};

/**
 * @brief Variant holding any of the PromQL value types
 */
struct Value {
    ValueType type;
    std::variant<std::monostate, Scalar, Vector, Matrix, String> data;

    Value() : type(ValueType::NONE), data(std::monostate{}) {}
    Value(Scalar s) : type(ValueType::SCALAR), data(s) {}
    Value(Vector v) : type(ValueType::VECTOR), data(v) {}
    Value(Matrix m) : type(ValueType::MATRIX), data(m) {}
    Value(String s) : type(ValueType::STRING), data(s) {}

    bool isScalar() const { return type == ValueType::SCALAR; }
    bool isVector() const { return type == ValueType::VECTOR; }
    bool isMatrix() const { return type == ValueType::MATRIX; }
    bool isString() const { return type == ValueType::STRING; }
    bool isNone() const { return type == ValueType::NONE; }

    const Scalar& getScalar() const { return std::get<Scalar>(data); }
    const Vector& getVector() const { return std::get<Vector>(data); }
    const Matrix& getMatrix() const { return std::get<Matrix>(data); }
    const String& getString() const { return std::get<String>(data); }
};

} // namespace promql
} // namespace prometheus
} // namespace tsdb
