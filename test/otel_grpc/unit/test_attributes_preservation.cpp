#include <gtest/gtest.h>
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include <iostream>

using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::collector::metrics::v1;

// Minimal test: Verify attributes survive protobuf copy
TEST(AttributesPreservation, BasicProtobufCopy) {
    // Create a data point with attributes
    NumberDataPoint point;
    point.set_time_unix_nano(1234567890);
    point.set_as_double(42.0);
    
    // Add 5 attributes
    for (int i = 0; i < 5; ++i) {
        auto* attr = point.add_attributes();
        attr->set_key("key" + std::to_string(i));
        attr->mutable_value()->set_string_value("value" + std::to_string(i));
    }
    
    // Verify attributes are set
    ASSERT_EQ(point.attributes_size(), 5) << "Attributes should be set before copy";
    
    // Copy via protobuf assignment (like benchmark does)
    NumberDataPoint copied_point;
    copied_point = point;
    
    // Verify attributes survived the copy
    ASSERT_EQ(copied_point.attributes_size(), 5) << "Attributes should survive protobuf copy";
    
    // Verify attribute values
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(copied_point.attributes(i).key(), "key" + std::to_string(i));
        EXPECT_EQ(copied_point.attributes(i).value().string_value(), "value" + std::to_string(i));
    }
}

// Test: Verify attributes survive when added to Gauge
TEST(AttributesPreservation, GaugeDataPointCopy) {
    // Create data point with attributes
    NumberDataPoint point;
    point.set_time_unix_nano(1234567890);
    point.set_as_double(42.0);
    
    // Add 10 attributes
    for (int i = 0; i < 10; ++i) {
        auto* attr = point.add_attributes();
        attr->set_key("attr" + std::to_string(i));
        attr->mutable_value()->set_string_value("val" + std::to_string(i));
    }
    
    ASSERT_EQ(point.attributes_size(), 10);
    
    // Create Gauge and add data point (like benchmark does)
    Gauge gauge;
    *gauge.mutable_data_points()->Add() = point;
    
    // Verify attributes survived
    ASSERT_EQ(gauge.data_points_size(), 1);
    const auto& copied_point = gauge.data_points(0);
    ASSERT_EQ(copied_point.attributes_size(), 10) << "Attributes should survive when added to Gauge";
}

// Test: Verify attributes survive when added to Metric
TEST(AttributesPreservation, MetricCopy) {
    // Create data point with 20 attributes
    NumberDataPoint point;
    point.set_time_unix_nano(1234567890);
    point.set_as_double(42.0);
    
    for (int i = 0; i < 20; ++i) {
        auto* attr = point.add_attributes();
        attr->set_key("key" + std::to_string(i));
        attr->mutable_value()->set_string_value("value" + std::to_string(i));
    }
    
    // Create Metric and add data point (like benchmark does)
    Metric metric;
    metric.set_name("test_metric");
    auto* gauge = metric.mutable_gauge();
    *gauge->mutable_data_points()->Add() = point;
    
    // Verify attributes survived
    ASSERT_EQ(metric.gauge().data_points_size(), 1);
    const auto& copied_point = metric.gauge().data_points(0);
    ASSERT_EQ(copied_point.attributes_size(), 20) << "Attributes should survive when added to Metric";
}

// Test: Verify attributes survive when added to ExportMetricsServiceRequest
TEST(AttributesPreservation, RequestCopy) {
    // Create data point with 40 attributes (like benchmark)
    NumberDataPoint point;
    point.set_time_unix_nano(1234567890);
    point.set_as_double(42.0);
    
    for (int i = 0; i < 40; ++i) {
        auto* attr = point.add_attributes();
        attr->set_key("label" + std::to_string(i));
        attr->mutable_value()->set_string_value("val" + std::to_string(i));
    }
    
    // Create full request structure (like benchmark)
    ExportMetricsServiceRequest request;
    auto* resource_metrics = request.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    
    Metric metric;
    metric.set_name("test_metric");
    auto* gauge = metric.mutable_gauge();
    *gauge->mutable_data_points()->Add() = point;
    
    *scope_metrics->add_metrics() = metric;
    
    // Verify attributes survived the full chain
    ASSERT_EQ(request.resource_metrics_size(), 1);
    ASSERT_EQ(request.resource_metrics(0).scope_metrics_size(), 1);
    ASSERT_EQ(request.resource_metrics(0).scope_metrics(0).metrics_size(), 1);
    
    const auto& final_point = request.resource_metrics(0)
                                  .scope_metrics(0)
                                  .metrics(0)
                                  .gauge()
                                  .data_points(0);
    
    ASSERT_EQ(final_point.attributes_size(), 40) 
        << "Attributes should survive full request structure copy";
    
    // Verify a few attribute values
    EXPECT_EQ(final_point.attributes(0).key(), "label0");
    EXPECT_EQ(final_point.attributes(0).value().string_value(), "val0");
    EXPECT_EQ(final_point.attributes(39).key(), "label39");
    EXPECT_EQ(final_point.attributes(39).value().string_value(), "val39");
}

