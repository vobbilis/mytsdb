# Test Coverage Strategy: 100% PromQL Compliance

## Objective
Define the strategy to implement ~1000 test cases covering all aspects of PromQL.

## Test Categories

### 1. Selector & Matcher Tests (100 cases)
- **Exact Match**: `metric{label="value"}`
- **Regex Match**: `metric{label=~"v.*"}`
- **Negative Exact**: `metric{label!="value"}`
- **Negative Regex**: `metric{label!~"v.*"}`
- **Multiple Matchers**: `metric{l1="v1", l2=~"v2", l3!="v3"}`
- **Internal Labels**: `__name__` selectors.
- **Empty Matchers**: Handling of empty string matches.

### 2. Range Vector Selectors (50 cases)
- **Time Ranges**: `[1m]`, `[5m]`, `[1h]`, `[1d]`.
- **Sub-second Ranges**: `[500ms]`.
- **Offset Modifiers**: `metric[5m] offset 1h`.
- **@ Modifier**: `metric[5m] @ 1600000000`.

### 3. Aggregation Operators (200 cases)
- **Basic**: `sum`, `min`, `max`, `avg`, `count`.
- **Advanced**: `stddev`, `stdvar`, `quantile`, `topk`, `bottomk`.
- **Grouping**: `by (...)`, `without (...)`.
- **Combinations**: `sum by (pod) (rate(...))`.
- **Edge Cases**: Aggregating empty vectors, single-sample vectors, NaN values.

### 4. PromQL Functions (300 cases)
- **Rate/Increase**: `rate`, `irate`, `increase`, `delta`, `idelta` (handle resets, extrapolation).
- **Math**: `abs`, `ceil`, `floor`, `round`, `sqrt`, `exp`, `ln`, `log2`, `log10`.
- **Time**: `day_of_month`, `day_of_week`, `days_in_month`, `hour`, `minute`, `month`, `year`, `timestamp`.
- **Label**: `label_replace`, `label_join`.
- **Over-Time**: `*_over_time` functions (all 12 variants).
- **Clamping**: `clamp`, `clamp_min`, `clamp_max`.
- **Sort**: `sort`, `sort_desc`.
- **Histogram**: `histogram_quantile`.

### 5. Binary Operators (150 cases)
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`, `^`.
- **Comparison**: `==`, `!=`, `>`, `<`, `>=`, `<=`.
- **Logical**: `and`, `or`, `unless`.
- **Vector Matching**:
    - One-to-One.
    - Many-to-One / One-to-Many (`group_left`, `group_right`).
    - Matching on specific labels (`on`, `ignoring`).

### 6. Subqueries (50 cases)
- Syntax: `rate(http_requests_total[5m])[30m:1m]`.
- Nested subqueries.
- Subqueries with offsets.

### 7. Complex Real-World Scenarios (150 cases)
- **SLO/SLI Queries**: Error budget burn rates.
- **Apdex Scores**: Complex histogram math.
- **K8s Mixin**: CPU saturation, memory limits, pod availability.
- **Alerting Rules**: `up == 0`, `rate(errors) / rate(requests) > 0.01`.

## Implementation Approach
1.  **Test Definition Format**: Define tests in a structured JSON/YAML format or C++ structs.
    ```cpp
    struct TestCase {
        string query;
        int64_t evaluation_time;
        ExpectedResult expected; // Value or Error
    };
    ```
2.  **Verification**:
    - Compare result type (Scalar, Vector, Matrix).
    - Compare series count and labels.
    - Compare sample values (with epsilon tolerance).
    - Compare timestamps.

## Source Material
- Prometheus `promql/testdata/` (official tests).
- `kube-prometheus` mixins.
- Public Grafana dashboards.
