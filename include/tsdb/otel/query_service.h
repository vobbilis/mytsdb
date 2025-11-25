#ifndef TSDB_OTEL_QUERY_SERVICE_H_
#define TSDB_OTEL_QUERY_SERVICE_H_

#ifdef HAVE_GRPC

#include <memory>
#include <grpcpp/grpcpp.h>
#include "tsdb/storage/storage.h"
#include "proto/gen/tsdb.grpc.pb.h"

namespace tsdb {
namespace otel {

/**
 * @brief gRPC service for querying time series data
 * 
 * Implements the TSDBService gRPC interface to allow clients to query
 * time series data from the running server.
 * 
 * Note: This class is defined in the header but the base class is only
 * forward-declared. The .cpp file includes the full proto headers.
 */
class QueryService : public tsdb::proto::TSDBService::Service {
public:
    explicit QueryService(std::shared_ptr<storage::Storage> storage);
    ~QueryService() override = default;
    
    grpc::Status GetSeries(
        grpc::ServerContext* context,
        const tsdb::proto::QueryParams* request,
        tsdb::proto::SeriesResponse* response) override;
    
    grpc::Status GetLabelNames(
        grpc::ServerContext* context,
        const tsdb::proto::QueryParams* request,
        tsdb::proto::LabelNamesResponse* response) override;
    
    grpc::Status GetLabelValues(
        grpc::ServerContext* context,
        const tsdb::proto::LabelValuesRequest* request,
        tsdb::proto::LabelValuesResponse* response) override;

private:
    std::shared_ptr<storage::Storage> storage_;
};

} // namespace otel
} // namespace tsdb

#endif // HAVE_GRPC

#endif // TSDB_OTEL_QUERY_SERVICE_H_

