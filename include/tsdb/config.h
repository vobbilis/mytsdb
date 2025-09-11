#ifndef TSDB_CONFIG_H_
#define TSDB_CONFIG_H_

// Project version
#define TSDB_VERSION_MAJOR 1
#define TSDB_VERSION_MINOR 0
#define TSDB_VERSION_PATCH 0
#define TSDB_VERSION "1.0.0"

// Feature flags
#define HAVE_SPDLOG
#define HAVE_GRPC
/* #undef HAVE_OTEL */
#define HAVE_GTEST

// Conditional includes
#ifdef HAVE_SPDLOG
#define TSDB_LOG_ENABLED 1
#else
#define TSDB_LOG_ENABLED 0
#endif

#ifdef HAVE_GRPC
#define TSDB_GRPC_ENABLED 1
#else
#define TSDB_GRPC_ENABLED 0
#endif

#ifdef HAVE_OTEL
#define TSDB_OTEL_ENABLED 1
#else
#define TSDB_OTEL_ENABLED 0
#endif

#endif // TSDB_CONFIG_H_
