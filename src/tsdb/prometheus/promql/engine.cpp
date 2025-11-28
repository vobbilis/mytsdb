#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/promql/parser.h"
#include "tsdb/prometheus/promql/lexer.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include <iostream>

namespace tsdb {
namespace prometheus {
namespace promql {

Engine::Engine(EngineOptions options) : options_(std::move(options)) {}

Engine::~Engine() = default;

QueryResult Engine::ExecuteInstant(const std::string& query, int64_t time) {
    auto start_time = std::chrono::high_resolution_clock::now();
    try {
        // 1. Parse the query
        auto parse_start = std::chrono::high_resolution_clock::now();
        Lexer lexer(query);
        Parser parser(lexer);
        auto ast = parser.ParseExpr();
        auto parse_end = std::chrono::high_resolution_clock::now();

        if (!ast) {
            return QueryResult{Value{}, {}, "Failed to parse query"};
        }

        // 2. Create Evaluator
        Evaluator evaluator(time, options_.lookback_delta.count(), options_.storage_adapter);

        // 3. Evaluate
        auto eval_start = std::chrono::high_resolution_clock::now();
        Value result = evaluator.Evaluate(ast.get());
        auto eval_end = std::chrono::high_resolution_clock::now();

        auto total_end = std::chrono::high_resolution_clock::now();
        
        auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(total_end - start_time).count();
        auto parse_duration = std::chrono::duration_cast<std::chrono::microseconds>(parse_end - parse_start).count();
        auto eval_duration = std::chrono::duration_cast<std::chrono::microseconds>(eval_end - eval_start).count();

        // Log slow queries (> 100ms)
        if (total_duration > 100000) {
            std::cout << "[SLOW QUERY] Duration: " << total_duration / 1000.0 << "ms "
                      << "(Parse: " << parse_duration / 1000.0 << "ms, "
                      << "Eval: " << eval_duration / 1000.0 << "ms) "
                      << "Query: " << query << std::endl;
        }

        return QueryResult{result, {}, ""};
    } catch (const std::exception& e) {
        return QueryResult{Value{}, {}, std::string("Execution error: ") + e.what()};
    }
}

QueryResult Engine::ExecuteRange(const std::string& query, int64_t start, int64_t end, int64_t step) {
    if (step <= 0) {
        return QueryResult{Value{}, {}, "Zero or negative step is not allowed"};
    }
    if (start > end) {
        return QueryResult{Value{}, {}, "Start time cannot be after end time"};
    }

    try {
        // 1. Parse the query
        Lexer lexer(query);
        Parser parser(lexer);
        auto ast = parser.ParseExpr();

        if (!ast) {
            return QueryResult{Value{}, {}, "Failed to parse query"};
        }

        // 2. Execute for each step
        // Note: This is a naive implementation. Real implementation should optimize this
        // by evaluating matrix selectors once and then slicing.
        
        Matrix resultMatrix;
        // We need to map series by their labels to construct the matrix
        // But for now, let's just implement the loop structure
        
        // TODO: Implement proper range evaluation optimization
        // For now, we return an error as it's not fully implemented
        return QueryResult{Value{}, {}, "Range queries not yet fully implemented"};

    } catch (const std::exception& e) {
        return QueryResult{Value{}, {}, std::string("Execution error: ") + e.what()};
    }
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
