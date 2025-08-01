// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: tsdb.proto

#include "tsdb.pb.h"
#include "tsdb.grpc.pb.h"

#include <functional>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/impl/channel_interface.h>
#include <grpcpp/impl/client_unary_call.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/message_allocator.h>
#include <grpcpp/support/method_handler.h>
#include <grpcpp/impl/rpc_service_method.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/impl/server_callback_handlers.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>
#include <grpcpp/support/sync_stream.h>
namespace tsdb {
namespace proto {

static const char* TSDBService_method_names[] = {
  "/tsdb.proto.TSDBService/GetLabelNames",
  "/tsdb.proto.TSDBService/GetLabelValues",
  "/tsdb.proto.TSDBService/GetSeries",
};

std::unique_ptr< TSDBService::Stub> TSDBService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< TSDBService::Stub> stub(new TSDBService::Stub(channel, options));
  return stub;
}

TSDBService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_GetLabelNames_(TSDBService_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_GetLabelValues_(TSDBService_method_names[1], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_GetSeries_(TSDBService_method_names[2], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status TSDBService::Stub::GetLabelNames(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams& request, ::tsdb::proto::LabelNamesResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::tsdb::proto::QueryParams, ::tsdb::proto::LabelNamesResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_GetLabelNames_, context, request, response);
}

void TSDBService::Stub::async::GetLabelNames(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams* request, ::tsdb::proto::LabelNamesResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::tsdb::proto::QueryParams, ::tsdb::proto::LabelNamesResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetLabelNames_, context, request, response, std::move(f));
}

void TSDBService::Stub::async::GetLabelNames(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams* request, ::tsdb::proto::LabelNamesResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetLabelNames_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::tsdb::proto::LabelNamesResponse>* TSDBService::Stub::PrepareAsyncGetLabelNamesRaw(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::tsdb::proto::LabelNamesResponse, ::tsdb::proto::QueryParams, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_GetLabelNames_, context, request);
}

::grpc::ClientAsyncResponseReader< ::tsdb::proto::LabelNamesResponse>* TSDBService::Stub::AsyncGetLabelNamesRaw(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncGetLabelNamesRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status TSDBService::Stub::GetLabelValues(::grpc::ClientContext* context, const ::tsdb::proto::LabelValuesRequest& request, ::tsdb::proto::LabelValuesResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::tsdb::proto::LabelValuesRequest, ::tsdb::proto::LabelValuesResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_GetLabelValues_, context, request, response);
}

void TSDBService::Stub::async::GetLabelValues(::grpc::ClientContext* context, const ::tsdb::proto::LabelValuesRequest* request, ::tsdb::proto::LabelValuesResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::tsdb::proto::LabelValuesRequest, ::tsdb::proto::LabelValuesResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetLabelValues_, context, request, response, std::move(f));
}

void TSDBService::Stub::async::GetLabelValues(::grpc::ClientContext* context, const ::tsdb::proto::LabelValuesRequest* request, ::tsdb::proto::LabelValuesResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetLabelValues_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::tsdb::proto::LabelValuesResponse>* TSDBService::Stub::PrepareAsyncGetLabelValuesRaw(::grpc::ClientContext* context, const ::tsdb::proto::LabelValuesRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::tsdb::proto::LabelValuesResponse, ::tsdb::proto::LabelValuesRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_GetLabelValues_, context, request);
}

::grpc::ClientAsyncResponseReader< ::tsdb::proto::LabelValuesResponse>* TSDBService::Stub::AsyncGetLabelValuesRaw(::grpc::ClientContext* context, const ::tsdb::proto::LabelValuesRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncGetLabelValuesRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status TSDBService::Stub::GetSeries(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams& request, ::tsdb::proto::SeriesResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::tsdb::proto::QueryParams, ::tsdb::proto::SeriesResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_GetSeries_, context, request, response);
}

void TSDBService::Stub::async::GetSeries(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams* request, ::tsdb::proto::SeriesResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::tsdb::proto::QueryParams, ::tsdb::proto::SeriesResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetSeries_, context, request, response, std::move(f));
}

void TSDBService::Stub::async::GetSeries(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams* request, ::tsdb::proto::SeriesResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetSeries_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::tsdb::proto::SeriesResponse>* TSDBService::Stub::PrepareAsyncGetSeriesRaw(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::tsdb::proto::SeriesResponse, ::tsdb::proto::QueryParams, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_GetSeries_, context, request);
}

::grpc::ClientAsyncResponseReader< ::tsdb::proto::SeriesResponse>* TSDBService::Stub::AsyncGetSeriesRaw(::grpc::ClientContext* context, const ::tsdb::proto::QueryParams& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncGetSeriesRaw(context, request, cq);
  result->StartCall();
  return result;
}

TSDBService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      TSDBService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< TSDBService::Service, ::tsdb::proto::QueryParams, ::tsdb::proto::LabelNamesResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](TSDBService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::tsdb::proto::QueryParams* req,
             ::tsdb::proto::LabelNamesResponse* resp) {
               return service->GetLabelNames(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      TSDBService_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< TSDBService::Service, ::tsdb::proto::LabelValuesRequest, ::tsdb::proto::LabelValuesResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](TSDBService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::tsdb::proto::LabelValuesRequest* req,
             ::tsdb::proto::LabelValuesResponse* resp) {
               return service->GetLabelValues(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      TSDBService_method_names[2],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< TSDBService::Service, ::tsdb::proto::QueryParams, ::tsdb::proto::SeriesResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](TSDBService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::tsdb::proto::QueryParams* req,
             ::tsdb::proto::SeriesResponse* resp) {
               return service->GetSeries(ctx, req, resp);
             }, this)));
}

TSDBService::Service::~Service() {
}

::grpc::Status TSDBService::Service::GetLabelNames(::grpc::ServerContext* context, const ::tsdb::proto::QueryParams* request, ::tsdb::proto::LabelNamesResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status TSDBService::Service::GetLabelValues(::grpc::ServerContext* context, const ::tsdb::proto::LabelValuesRequest* request, ::tsdb::proto::LabelValuesResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status TSDBService::Service::GetSeries(::grpc::ServerContext* context, const ::tsdb::proto::QueryParams* request, ::tsdb::proto::SeriesResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace tsdb
}  // namespace proto

