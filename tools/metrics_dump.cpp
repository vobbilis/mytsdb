#include <iostream>
#include <iomanip>
#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void query_and_print_metric(httplib::Client& cli, const std::string& metric_name, const std::string& unit = "") {
    auto res = cli.Get(("/api/v1/query?query=" + metric_name).c_str());
    if (res && res->status == 200) {
        try {
            auto j = json::parse(res->body);
            if (j["status"] == "success" && !j["data"]["result"].empty()) {
                double value = std::stod(j["data"]["result"][0]["value"][1].get<std::string>());
                std::cout << std::left << std::setw(50) << metric_name << ": " 
                          << std::fixed << std::setprecision(6) << value;
                if (!unit.empty()) {
                    std::cout << " " << unit;
                }
                std::cout << std::endl;
            } else {
                std::cout << std::left << std::setw(50) << metric_name << ": " << "N/A" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << std::left << std::setw(50) << metric_name << ": " << "PARSE ERROR" << std::endl;
        }
    } else {
        std::cout << std::left << std::setw(50) << metric_name << ": " << "HTTP ERROR" << std::endl;
    }
}

int main(int argc, char** argv) {
    std::string http_addr = "localhost:9090";
    if (argc > 1) {
        http_addr = argv[1];
    }

    auto pos = http_addr.find(':');
    std::string host = http_addr.substr(0, pos);
    int port = std::stoi(http_addr.substr(pos + 1));

    httplib::Client cli(host, port);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(10, 0);

    std::cout << "\n=== MyTSDB Performance Metrics ===" << std::endl;
    std::cout << "\n--- Write Path Metrics ---" << std::endl;
    query_and_print_metric(cli, "mytsdb_storage_writes_total", "writes");
    query_and_print_metric(cli, "mytsdb_write_otel_conversion_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_grpc_handling_seconds_total", "seconds");
    
    std::cout << "\n--- Detailed Write Breakdown ---" << std::endl;
    query_and_print_metric(cli, "mytsdb_write_wal_write_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_series_id_calc_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_index_insert_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_series_creation_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_map_insert_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_sample_append_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_cache_update_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_block_seal_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_block_persist_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_mutex_lock_seconds_total", "seconds");

    std::cout << "\n--- Detailed OTEL Conversion Breakdown ---" << std::endl;
    query_and_print_metric(cli, "mytsdb_write_otel_resource_processing_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_otel_scope_processing_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_otel_metric_processing_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_otel_label_conversion_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_write_otel_point_conversion_seconds_total", "seconds");

    std::cout << "\n--- Read Path Metrics ---" << std::endl;
    query_and_print_metric(cli, "mytsdb_read_total", "reads");
    query_and_print_metric(cli, "mytsdb_read_duration_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_read_index_search_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_read_block_lookup_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_read_block_read_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_read_decompression_seconds_total", "seconds");
    query_and_print_metric(cli, "mytsdb_read_samples_scanned_total", "samples");
    query_and_print_metric(cli, "mytsdb_read_blocks_accessed_total", "blocks");
    query_and_print_metric(cli, "mytsdb_read_cache_hits_total", "hits");

    std::cout << "\n--- Storage Metrics ---" << std::endl;
    query_and_print_metric(cli, "mytsdb_storage_reads_total", "reads");
    query_and_print_metric(cli, "mytsdb_storage_cache_hits_total", "hits");
    query_and_print_metric(cli, "mytsdb_storage_cache_misses_total", "misses");
    query_and_print_metric(cli, "mytsdb_storage_bytes_written_total", "bytes");
    query_and_print_metric(cli, "mytsdb_storage_bytes_read_total", "bytes");
    query_and_print_metric(cli, "mytsdb_storage_net_memory_usage_bytes", "bytes");

    std::cout << "\n--- Query Metrics ---" << std::endl;
    query_and_print_metric(cli, "mytsdb_query_count_total", "queries");
    query_and_print_metric(cli, "mytsdb_query_errors_total", "errors");
    query_and_print_metric(cli, "mytsdb_query_duration_seconds_total", "seconds");

    std::cout << std::endl;
    return 0;
}
