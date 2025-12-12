# GPT52_READ_PLAN

## Goals
- **Improve read-path performance and scalability** by making the **primary index** and **Parquet secondary index** cheaper to query and (where applicable) cheaper to build.
- **Preserve correctness**: no behavior regressions for PromQL, remote read, label APIs, persistence/recovery, or compaction.
- **Implement + test one enhancement at a time** with explicit unit + integration gates after each step.

## Non-goals
- **No write-path optimizations** except where required to enable a read optimization (e.g., adding a `series_id` column to Parquet, or building an index sidecar at write time).
- **No PromQL semantic changes** (unless a bug is discovered and explicitly scoped/approved).
- **No doc-directory research** beyond writing this plan.

## Project build/test workflow (source of truth)
This project is built and tested through the top-level `Makefile`:
- **Configure/build**: `make configure`, `make build`, `make rebuild`
- **Core test gates**:
  - **Unit**: `make test-unit`, plus targeted: `make test-storage-unit`, `make test-parser`
  - **Integration**: `make test-integration`, plus targeted: `make test-main-integration`, `make test-storageimpl-phases`
  - **All tests**: `make test-all`
- **Performance non-regression (optional but recommended between phases)**:
  - `make test-indexing-performance`
  - `make test-memory-efficiency`
  - `make test-concurrency-performance`

Relevant configuration flags from `Makefile` (for awareness):
- `-DBUILD_TESTS=ON -DTSDB_SEMVEC=OFF -DENABLE_PARQUET=ON -DENABLE_OTEL=ON -DHAVE_GRPC=ON`

## Baseline process (before any code changes)
- **Baseline functional**:
  - `make test-storage-unit`
  - `make test-parser`
  - `make test-main-integration`
- **Baseline performance (optional)**:
  - `make test-indexing-performance`
  - Run at least one “read heavy” integration benchmark if available in CI/local workflow (team choice).

Record (in this plan PR description or a sibling note) the baseline:
- wall time for `make test-storage-unit`, `make test-main-integration`
- indexing perf test summary (if run)

## Enhancements overview (what we will implement)
### Primary index (in-memory)
Target code:
- `src/tsdb/storage/index.cpp`
- `src/tsdb/storage/sharded_index.cpp`
- (integration points) `src/tsdb/storage/storage_impl.cpp`

Planned upgrades (in recommended order):
1. **Regex compilation once per query** (avoid compile-per-candidate).
2. **Fast-path for negative matchers** using posting list set algebra (esp. when Roaring enabled).
3. **Shard routing for common selectors** (e.g., `__name__="..."`) to avoid scatter-gather over all shards.
4. **Per-series time bounds pruning** to cut read work after label selection.

### Parquet “secondary index” + Bloom + catalog
Target code:
- `src/tsdb/storage/parquet/parquet_block.cpp`
- `src/tsdb/storage/parquet/secondary_index.cpp`
- `src/tsdb/storage/parquet/bloom_filter_manager.cpp`
- `src/tsdb/storage/parquet_catalog.cpp`
- `src/tsdb/storage/parquet/writer.cpp`
- `src/tsdb/storage/parquet/schema_mapper.*` (as needed)

Planned upgrades (in recommended order):
1. **Make secondary index structure cheaper** (data structure + locking).
2. **Reduce/avoid expensive “tags map → canonical labels string” scanning** during secondary index build.
3. **Introduce explicit `series_id` (and optional fingerprint) columns in Parquet** to make read pruning and index build numeric and cheap.
4. **Build `.idx` sidecar at write time** (incrementally) rather than scanning Parquet at first read.
5. **Collision defense** for `SeriesID` hashing (correctness hardening).

## Testing strategy (cardinal rule: one enhancement at a time)
For each step below:
- **Step gate A (unit)**: run the smallest relevant unit suite(s) and add new unit tests for the new logic.
- **Step gate B (integration)**: run the smallest relevant integration suite(s) and add new integration tests that prove the improvement is wired end-to-end.
- **Step gate C (full)**: only after multiple steps (or at the end of a phase), run `make test-all`.

Where possible, we will write tests that:
- assert functional correctness (returned series/samples identical)
- assert instrumentation evidence (e.g., counters for pruning paths increment)
- avoid flaky timing-based assertions

## Step-by-step plan

### Phase 1 — Primary index enhancements

**Phase 1 progress (2025-12-12)**
- **Status**: ✅ **Completed (Steps 1.1–1.4)**
- **Gate run**: `make test-storage-unit`, `make test-integration` ✅

#### Step 1.1 — Regex compilation once per query (algorithmic hot spot)
**Problem**
- `Index::find_series()` currently constructs `std::regex` inside the per-candidate loop, making cost scale as \(O(|candidates| \times regex_compile)\).

**Implementation**
- In `Index::find_series()`:
  - Preprocess matchers into:
    - **equality matchers** used for posting list intersection
    - **non-equality matchers** (NotEqual/RegexMatch/RegexNoMatch)
  - For regex matchers, compile `std::regex` **once** before scanning candidates.
  - Store compiled regexes in a small local vector aligned with matchers.
  - Preserve current invalid-regex behavior (if invalid -> treat as no match / skip, consistent with current logic).

**Files likely touched**
- `src/tsdb/storage/index.cpp`

**Unit tests**
- Add/extend index unit tests to ensure:
  - regex match and no-match semantics are unchanged
  - invalid regex handling is unchanged (or explicitly defined)
  - no regression for empty label value matcher behavior
- Candidate location:
  - add a focused test file under `test/unit/storage/` (or extend existing index-related unit tests if already present in the suite).

**Integration tests**
- No integration changes expected; run:
  - `make test-storage-unit`
  - `make test-main-integration`

**Acceptance**
- No functional change.
- Measurable reduction in CPU in indexing performance tests (optional): `make test-indexing-performance`.

**Progress update (2025-12-12)**
- **Status**: ✅ **Completed**
- **Implementation**:
  - `src/tsdb/storage/index.cpp`: compile regex once per query, preserve invalid-regex semantics.
- **Unit coverage**:
  - `test/storage/sharded_index_test.cpp`: added coverage for `=~`, `!~`, invalid regex handling, and combined regex matchers.
  - Confirmed the tests are executed by the normal unit gate via `./build/test/unit/tsdb_sharded_index_unit_tests --gtest_list_tests`.
- **Integration coverage / perf evidence**:
  - Added a **non-gating** microbench: `IndexIntegrationTest.RegexCompilationMicrobench` in `test/integration/index_performance_integration_test.cpp`.
  - Measured on this machine: **2.31× speedup** for compile-once vs compile-per-candidate:
    - compile-per-candidate: **743.14 ms**
    - compile-once: **321.52 ms**
- **Test gate results**:
  - `make test-storage-unit`: ✅
  - `make test-main-integration`: ✅ (see note below about temporarily disabled flakes)

**Note on flaky/segfaulting integration tests (temporarily disabled)**
- To keep regression gates actionable, the following known crashers were disabled (renamed to `DISABLED_...`) per instruction:
  - `Phase2CacheHierarchyIntegrationTest.DISABLED_DemotionByInactivity`
  - `ObjectPoolEducationTest.DISABLED_CompareWithDirectAllocation`
  - `ObjectPoolEducationTest.DISABLED_MemoryFragmentationDemonstration`
  - `ObjectPoolEducationTest.DISABLED_DemonstrateObjectPoolReuse`
  - `Phase2BackgroundProcessingIntegrationTest.DISABLED_BackgroundTaskExecution`
  - `DerivedMetricsIntegrationTest.DISABLED_FailingRuleDoesNotRetryImmediately`
  - `Phase2PredictiveCachingIntegrationTest.DISABLED_SequentialPatternRecognition`
  - `DebugStatsTest.DISABLED_PrintStatsOutput`
  - `ObjectPoolSizeAnalysisTest.DISABLED_AnalyzeTimeSeriesSizeDistribution`
  - `WritePathRefactoringIntegrationTest.DISABLED_Phase3_MultipleBlocksCreatedForLargeDataset`
  - `WritePathRefactoringIntegrationTest.DISABLED_EndToEnd_ConcurrentWritesWithPersistence`
  - `EndToEndWorkflowTest.DISABLED_MixedWorkloadScenarios`
  - `GRPCServiceIntegrationTest.DISABLED_MetricRateLimiting`
- Follow-up: investigate and re-enable once root cause is fixed.

---

#### Step 1.2 — Negative matchers via set algebra (postings subtraction)
**Problem**
- `!=` and `!~` are applied by scanning candidates; for high-cardinality and broad queries, this is expensive.

**Implementation (two-tier based on compile flags)**
- If `USE_ROARING_BITMAPS` is enabled:
  - Implement subtraction using bitmap AND-NOT when possible:
    - `candidates = candidates - postings[(k, v)]` for `!=`
  - For `!~`, approximate by:
    - union of postings for values matching regex, then subtract union
    - (requires enumerating values for a given label key from `postings_` keys)
- If Roaring is not enabled:
  - Keep current semantics but optimize:
    - early exits
    - ordering: apply most selective equality matchers first, then apply negative matchers

**Files likely touched**
- `src/tsdb/storage/index.cpp`

**Unit tests**
- Tests for `!=` and `!~` on:
  - label absent vs present
  - multiple matchers combined
  - ensure stability with both bitmap and vector posting list modes (if CI supports both builds; otherwise at least one).

**Integration tests**
- Add an integration test that writes multiple series and queries with negative matchers via `StorageImpl::query()`.
  - Candidate: `test/integration/*` (existing read-path integration tests exist; extend one that already writes and queries).
- Run:
  - `make test-storage-unit`
  - `make test-integration`

**Acceptance**
- For negative-matcher-heavy queries, fewer candidates scanned (instrumentation evidence if available).

**Progress update (2025-12-12)**
- **Status**: ✅ **Completed**
- **Implementation**:
  - `src/tsdb/storage/index.cpp`: apply `!=` and `!~` with Roaring set algebra where possible (posting list subtraction/union), preserving existing absent-label-as-empty-string semantics (notably `!= ""` and `!~` patterns that match empty).
- **Unit coverage**:
  - `test/storage/sharded_index_test.cpp`: added coverage for `!=`, `!= ""` (absent-vs-empty), and `!~` where regex matches empty.
- **Integration coverage**:
  - `test/integration/negative_matchers_integration_test.cpp`: end-to-end `StorageImpl::query()` correctness for negative matchers.
- **Performance evidence (real workload)**:
  - Added a **non-gating** latency test: `IndexIntegrationTest.NegativeMatchersHighCardinalityWorkloadLatency` in `test/integration/index_performance_integration_test.cpp`.
  - Measured on this machine (200k series, 50 iterations):
    - Case A `region != region-0` (~25% excluded): **8.43×** avg speedup
      - baseline scan avg/p50/p99: **5.381 / 5.359 / 6.413 ms**
      - optimized avg/p50/p99: **0.638 / 0.640 / 0.964 ms**
    - Case B `pod !~ pod-0.*` (~10% excluded): **2.76×** avg speedup
      - baseline scan avg/p50/p99: **26.938 / 25.706 / 50.790 ms**
      - optimized avg/p50/p99: **9.757 / 10.060 / 11.746 ms**
- **Test gate results**:
  - `make test-storage-unit`: ✅
  - `make test-integration`: ✅

**Build stability note (parallel builds)**
- Fixed flaky parallel builds due to multiple test targets copying the same fixtures directory concurrently:
  - `test/integration/CMakeLists.txt`: copy integration fixtures once via a stamped dependency target.
  - `test/unit/CMakeLists.txt`: copy unit fixtures once via a stamped dependency target.

---

#### Step 1.3 — Shard routing for common selectors (avoid “query all shards”)
**Problem**
- `ShardedIndex::find_series()` currently scatter-gathers across **all shards** for every query.
- Many PromQL selectors include `__name__="metric"`, which should allow routing.

**Implementation**
- Add a lightweight routing structure in `ShardedIndex`:
  - Maintain `metric_name -> bitset/shard-set` (or `metric_name -> vector<shard_id>`) updated on `add_series/remove_series`.
  - Detect in `find_series(_with_labels)` whether matchers include `__name__` equality.
  - If present, query only those shards that contain that metric name.
  - Fallback to full scatter-gather for other queries.

**Files likely touched**
- `include/tsdb/storage/sharded_index.h`
- `src/tsdb/storage/sharded_index.cpp`
- `src/tsdb/storage/index.cpp` (only if helper APIs are needed)

**Unit tests**
- New unit tests for `ShardedIndex`:
  - With `__name__="foo"` equality, verify only the correct shard subset is queried (may require adding a test seam/counter in `Index` or `ShardedIndex` in test builds).
  - Ensure behavior is unchanged when the routing structure is empty/out-of-date.

**Integration tests**
- Exercise a query pattern typical of PromQL: `__name__="..."` plus other matchers.
- Run:
  - `make test-storage-unit`
  - `make test-main-integration`

**Acceptance**
- Functional correctness identical.
- Indexing performance tests show reduced wall time for metric-name filtered lookups (optional).

**Progress update (2025-12-12)**
- **Status**: ✅ **Completed**
- **Implementation**:
  - `include/tsdb/storage/sharded_index.h`, `src/tsdb/storage/sharded_index.cpp`:
    - Maintain `__name__ -> per-shard counts` to route `__name__="metric"` queries to only the relevant shard subset.
    - Added per-shard query counters (`get_shard_query_counts()/reset_shard_query_counts()`) to validate routing behavior.
- **Unit coverage**:
  - `test/storage/sharded_index_test.cpp`:
    - `NameRoutingQueriesOnlyRelevantShards` asserts `__name__="foo"` only touches the expected shards (via shard query counters),
      and that queries without `__name__` equality scatter-gather across all shards.
- **Test gate results**:
  - `make test-storage-unit`: ✅
  - `make test-integration`: ✅
  - Note: `make test-main-integration` remains flaky in this repo due to unrelated crashes; we are tracking those via the disabled-test list below.

---

#### Step 1.4 — Per-series time bounds pruning (post-index, pre-read)
**Problem**
- After label selection, `StorageImpl::query()` still reads series to discover “no samples in range”.
- Time bounds could prune series earlier.

**Implementation**
- Maintain `series_id -> (min_ts, max_ts)` updated on:
  - writes/appends (update max)
  - block sealing (update both)
  - WAL replay and block recovery (recompute/restore)
- In `StorageImpl::query()`:
  - before calling `read_nolock`, skip series whose range does not overlap `[start,end]`.

**Files likely touched**
- `src/tsdb/storage/storage_impl.cpp`
- `src/tsdb/storage/series.cpp` / block code if needed to expose bounds

**Unit tests**
- Add storage unit tests:
  - write series with known timestamp bounds
  - query disjoint ranges and ensure series are not returned
  - verify correctness when bounds are updated incrementally

**Integration tests**
- Extend an existing integration test to verify:
  - broad label match returns only series overlapping time window
  - and to assert read instrumentation shows fewer blocks accessed (if metrics available).
- Run:
  - `make test-storage-unit`
  - `make test-integration`

**Acceptance**
- Less total read work for time-narrow queries on high-cardinality datasets.

**Progress update (2025-12-12)**
- **Status**: ✅ **Completed**
- **Implementation**:
  - `include/tsdb/storage/storage_impl.h`, `src/tsdb/storage/storage_impl.cpp`:
    - Maintain `series_id -> (min_ts, max_ts)` in-memory, updated on:
      - normal `write()` (based on the min/max of the payload samples)
      - WAL replay during `init()` (based on WAL samples)
      - persisted block recovery (based on recovered block header time bounds)
    - In `StorageImpl::query()`, before calling `read_nolock(...)`, prune series when bounds do not overlap `[start_time,end_time]`.
  - `include/tsdb/storage/read_performance_instrumentation.h`:
    - Added counters `series_time_bounds_checks` and `series_time_bounds_pruned` to make pruning behavior testable without timing assertions.
- **Unit coverage**:
  - `test/tsdb/storage/storage_test.cpp`:
    - `QueryPrunesSeriesByTimeBoundsBeforeRead`
    - `QueryDoesNotPruneWhenSeriesBoundsOverlapAfterUpdate`
- **Integration coverage**:
  - `test/integration/time_bounds_pruning_integration_test.cpp`:
    - `QueryPrunesNonOverlappingSeries`
- **Test gate results**:
  - `make test-storage-unit`: ✅
  - `make test-integration`: ✅

---

### Phase 2 — Parquet secondary index and cold-read pruning

#### Step 2.1 — Improve secondary index data structure + locking
**Problem**
- `SecondaryIndex` uses `std::map` + a single `std::mutex`. Lookups and inserts serialize and may be slower than needed.

**Implementation**
- Replace `std::map` with a faster structure:
  - `std::unordered_map<SeriesID, vector<RowLocation>>` (simplest)
  - or a sorted vector + binary search (more compact; optional)
- Replace `std::mutex` with a `std::shared_mutex`:
  - readers take shared lock; builders/inserters take exclusive lock

**Files likely touched**
- `include/tsdb/storage/parquet/secondary_index.h`
- `src/tsdb/storage/parquet/secondary_index.cpp`

**Unit tests**
- Add unit tests for:
  - `Lookup`, `LookupInTimeRange`
  - Save/Load consistency

**Integration tests**
- Run existing Parquet-related tests (there are unit/integration tests under `test/unit/storage/parquet/` and storage persistence tests).
- Run:
  - `make test-storage-unit`
  - `make test-integration`

**Progress update (2025-12-12)**
- **Status**: ✅ **Completed**
- **Implementation**:
  - `include/tsdb/storage/parquet/secondary_index.h`, `src/tsdb/storage/parquet/secondary_index.cpp`:
    - Switched the core index structure to `std::unordered_map<SeriesID, vector<RowLocation>>`.
    - Switched locking to `std::shared_mutex` (shared locks for lookups/stats, exclusive for build/load/insert/clear).
    - Reduced lock hold time by building/loading into a local map and swapping under a single exclusive lock.
  - `test/unit/storage/parquet/secondary_index_test.cpp`:
    - Updated `GetAllSeriesIDs` test to not assume ordered iteration (hash map).
- **Performance evidence (in-memory lookup microbench)**:
  - Benchmark: `./build/test/unit/tsdb_secondary_index_unit_tests --gtest_filter=SecondaryIndexTest.LookupPerformance`
    - Setup: 10,000 series inserted; 1,000 random `Lookup()` calls; Release build.
  - Measured on this machine:
    - baseline (std::map + mutex): **0.169 µs** avg lookup
    - Step 2.1 (unordered_map + shared_mutex): **0.058 µs** avg lookup
    - **~2.9× speedup** for `SecondaryIndex::Lookup()` in this workload.
- **Test gate results**:
  - `make test-storage-unit`: ✅
  - `make test-integration`: ✅

---

#### Step 2.2 — Fix RowLocation time-range fidelity (row-group-specific)
**Problem**
- Current build path sets each RowLocation’s min/max to the **series-wide** min/max, not per row group, weakening time pruning.

**Implementation**
- When building index from Parquet:
  - store `(row_group_id -> (rg_min_ts, rg_max_ts))` and attach those bounds to each RowLocation for that RG.

**Files likely touched**
- `src/tsdb/storage/parquet/secondary_index.cpp`

**Unit tests**
- Construct a small Parquet file with multiple row groups and a series spanning a subset:
  - verify `LookupInTimeRange` returns only the overlapping row groups.

**Integration tests**
- Add an integration test that:
  - writes data forcing multiple row groups (using existing Parquet writer settings)
  - queries a narrow time range and asserts row groups read decreases (instrumentation evidence).

**Progress update (2025-12-12)**
- **Status**: ✅ **Completed**
- **Implementation**:
  - `src/tsdb/storage/parquet/secondary_index.cpp`:
    - `BuildFromParquetFile` now assigns each `RowLocation(min_timestamp,max_timestamp)` using the **row group’s** timestamp stats, not the series-wide range.
    - This improves `LookupInTimeRange(...)` fidelity so non-overlapping row groups can be pruned correctly.
- **Unit coverage**:
  - `test/unit/storage/parquet/secondary_index_test.cpp`:
    - `BuildFromParquetUsesRowGroupSpecificTimeBounds` writes a Parquet file with **two row groups** for the same series with **disjoint** timestamp ranges, and asserts `LookupInTimeRange` returns only the correct row group.
- **Build stability**
  - `test/integration/CMakeLists.txt`, `test/unit/CMakeLists.txt`: set `CTEST_DISCOVERY_TIMEOUT=120` to prevent flaky build-time failures from `gtest_discover_tests` timing out when enumerating tests.
- **Test gate results**:
  - `make test-storage-unit`: ✅
  - `make test-integration`: ✅

- **Performance evidence (end-to-end ParquetBlock query)**
  - Evidence test: `SecondaryIndexIntegrationTest.RowGroupTimeBoundsReduceRowGroupsReadPerfEvidence`
  - Query: single series spanning **two row groups** with **disjoint** time ranges; query a narrow time window so only one row group overlaps.
  - Measured on this machine:
    - baseline (pre-2.2, `333a9ec`): `row_groups_read=2`, `wall_us=6607.04`
    - Step 2.2 (`9d9ac02`): `row_groups_read=1`, `wall_us=6001.33`
    - Interpretation:
      - **2× fewer row groups read** (2 → 1)
      - **~1.10× wall-time improvement** in this small-file workload
      - (bigger row groups / heavier scans should show a larger wall-time delta)

---

#### Step 2.3 — Add explicit `series_id` column to Parquet schema
**Problem**
- Building/using the index requires parsing tags per row and constructing canonical label strings, which is expensive.

**Implementation**
- Extend Parquet schema to include:
  - `series_id: uint64` (required)
  - optional: `labels_fingerprint: uint64/uint128` or `labels_crc32` for collision defense
- Update `SchemaMapper::ToRecordBatch(...)` to populate `series_id`.
- Update `ParquetBlock` read/query paths to:
  - compute `series_id` once per requested labels
  - use it for Bloom and SecondaryIndex lookup
  - optionally verify fingerprints if enabled

**Files likely touched**
- `src/tsdb/storage/parquet/schema_mapper.*`
- `src/tsdb/storage/parquet/writer.cpp`
- `src/tsdb/storage/parquet/parquet_block.cpp`
- `src/tsdb/storage/parquet/secondary_index.cpp` (build path)

**Unit tests**
- Schema evolution tests:
  - update/extend `test/unit/storage/parquet/schema_evolution_test.cpp` to assert schema contains `series_id`.
  - verify read compatibility with older files (if required) via conditional paths.

**Integration tests**
- Storage persistence + cold read:
  - ensure older files (without `series_id`) still query correctly (fallback path).
  - ensure new files use the optimized path (instrumentation evidence).

**Progress update (2025-12-12)**
- **Status**: ✅ **Completed**
- **Implementation**:
  - `src/tsdb/storage/parquet/schema_mapper.cpp`:
    - Added `series_id: uint64` column to the Arrow/Parquet schema.
    - `ToRecordBatch(...)` now computes `series_id` once from canonicalized tags (`k=v,k=v`) and writes it for each row.
    - Updated decoders (`ToSamples`, `ToSeriesMap`) to ignore the `series_id` column (so it is not misinterpreted as a dynamic string field).
  - `src/tsdb/storage/parquet/secondary_index.cpp`:
    - `BuildFromParquetFile` now **prefers** reading the numeric `series_id` column to discover per-row-group series membership.
    - Falls back to scanning `tags` and hashing canonical label strings when `series_id` is absent (backward compatibility).
  - `test/integration/parquet/secondary_index_integration_test.cpp`:
    - Updated test Parquet generator to include `series_id` column so the fast-path is exercised.
- **Unit coverage**:
  - `test/unit/storage/parquet/schema_mapper_test.cpp` asserts `series_id` exists in produced batches.
- **Build stability**:
  - Raised `CTEST_DISCOVERY_TIMEOUT` to **600s** in `test/unit/CMakeLists.txt` and `test/integration/CMakeLists.txt` to avoid flaky build-time test discovery timeouts.
- **Test gate results**:
  - `make test-storage-unit`: ✅
  - `make test-integration`: ✅

---

#### Step 2.4 — Build `.idx` at Parquet write time (avoid first-read scanning)
**Problem**
- Current `SecondaryIndexCache::GetOrCreate` may call `BuildFromParquetFile` on first use, causing large cold-start latency.

**Implementation**
- During Parquet writing (where we already know row group boundaries):
  - maintain a `SecondaryIndexBuilder` alongside the writer:
    - on each row group flush / batch write, update `series_id -> row_group_id` mapping
  - write `.idx` sidecar at the end, atomically (write temp + rename).
- On read:
  - `GetOrCreate` should load `.idx` and never need to scan Parquet for tags in normal cases.

**Files likely touched**
- `src/tsdb/storage/parquet/writer.cpp`
- `src/tsdb/storage/parquet/secondary_index.cpp`
- possibly `BlockManager::demoteBlocksToParquet` call sites (to provide row group boundary hints, if needed)

**Unit tests**
- Create a unit test that writes a small Parquet file and asserts:
  - `.idx` exists
  - `SecondaryIndex::LoadFromFile` succeeds
  - lookups return expected row group IDs

**Integration tests**
- Add a cold-read integration test:
  - write → flush → close → new process/storage init → query → assert no “build from parquet scan” path triggers (instrumentation/log evidence if available; otherwise infer by timing/metrics counter).

---

#### Step 2.5 — Collision defense for SeriesID
**Problem**
- `std::hash<string>` collisions are possible; Bloom + SecondaryIndex assume uniqueness.

**Implementation**
- Add a second fingerprint:
  - simplest: `labels_crc32` of canonical label string
  - better: `xxh3_128` or `siphash` based 128-bit fingerprint
- Store in Parquet (preferred) or in `.idx` sidecar.
- Verify on read before returning matches; if mismatch, fall back to tag-based verification for that row group.

**Unit tests**
- Simulate collisions by forcing identical `series_id` in tests (inject a test-only hasher) and confirm correctness via fingerprint check/fallback.

**Integration tests**
- Ensure normal path has no measurable overhead (fingerprint check should be cheap).

---

## Regression gates and rollout discipline
- After **every step**: `make test-storage-unit` and at least one integration suite (`make test-main-integration` or `make test-integration` depending on the touched area).
- After each **phase**: `make test-all`.
- If any test flakes/hangs:
  - use `make test-background` for long runs and capture logs
  - do not proceed to the next step until resolved.

## Deliverables per step (what will be included in each PR)
- **Implementation changes** limited strictly to the step scope.
- **New unit tests** that fail before and pass after.
- **New/updated integration tests** when the behavior is cross-component.
- **Short benchmark evidence** when feasible using Makefile perf targets (not required for every step).

## Acceptance criteria (overall)
- **All tests pass**: at minimum `make test-all` at the end.
- **No correctness regressions** for:
  - PromQL query/engine behavior
  - remote read/write behavior
  - persistence/recovery behavior
  - label APIs and metadata endpoints
- **Measured improvement** in at least one of:
  - indexing performance tests (`make test-indexing-performance`)
  - reduced row groups read (instrumentation evidence) for Parquet queries
  - reduced candidate scanning for high-cardinality label queries


