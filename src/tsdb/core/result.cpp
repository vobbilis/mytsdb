#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace core {

// Explicit template instantiations
template class Result<std::vector<uint8_t>>;
template class Result<std::string>;

}  // namespace core
}  // namespace tsdb 