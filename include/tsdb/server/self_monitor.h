#pragma once

#include "tsdb/storage/storage.h"
#include "tsdb/storage/background_processor.h"
#include <memory>
#include <atomic>

namespace tsdb {
namespace server {

class SelfMonitor {
public:
    SelfMonitor(std::shared_ptr<storage::Storage> storage, 
                std::shared_ptr<storage::BackgroundProcessor> background_processor);
    
    void Start();
    void Stop();

private:
    void ScrapeAndWrite();
    
    std::shared_ptr<storage::Storage> storage_;
    std::shared_ptr<storage::BackgroundProcessor> background_processor_;
    std::atomic<bool> running_{false};
};

} // namespace server
} // namespace tsdb
