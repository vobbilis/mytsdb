#include <iostream>
#include <string>
#include <map>
#include <sstream>
#include <iomanip>

// Simulate the Labels class behavior
class Labels {
private:
    std::map<std::string, std::string> labels_;
    
public:
    void add(const std::string& name, const std::string& value) {
        labels_[name] = value;
    }
    
    std::string to_string() const {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (const auto& [name, value] : labels_) {
            if (!first) {
                oss << ", ";
            }
            oss << name << "=\"" << value << "\"";
            first = false;
        }
        oss << "}";
        return oss.str();
    }
    
    bool operator==(const Labels& other) const {
        return labels_ == other.labels_;
    }
};

// Simulate the series ID calculation
std::size_t calculate_series_id(const Labels& labels) {
    std::hash<std::string> hasher;
    std::string label_string = labels.to_string();
    return hasher(label_string);
}

int main() {
    std::cout << "🔍 DEBUGGING SERIES ID CALCULATION ISSUE\n";
    std::cout << "========================================\n\n";
    
    // Test 1: Same labels created in different order
    std::cout << "Test 1: Same labels created in different order\n";
    std::cout << "------------------------------------------------\n";
    
    Labels labels1;
    labels1.add("__name__", "boundary_large");
    labels1.add("test", "phase1");
    labels1.add("pool_test", "true");
    labels1.add("size", "large");
    
    Labels labels2;
    labels2.add("size", "large");
    labels2.add("pool_test", "true");
    labels2.add("test", "phase1");
    labels2.add("__name__", "boundary_large");
    
    std::cout << "Labels1: " << labels1.to_string() << std::endl;
    std::cout << "Labels2: " << labels2.to_string() << std::endl;
    std::cout << "Are equal: " << (labels1 == labels2) << std::endl;
    std::cout << "Series ID 1: " << calculate_series_id(labels1) << std::endl;
    std::cout << "Series ID 2: " << calculate_series_id(labels2) << std::endl;
    std::cout << "IDs match: " << (calculate_series_id(labels1) == calculate_series_id(labels2)) << std::endl;
    
    // Test 2: Multiple iterations to see if order is consistent
    std::cout << "\nTest 2: Multiple iterations to check consistency\n";
    std::cout << "------------------------------------------------\n";
    
    for (int i = 0; i < 5; ++i) {
        Labels test_labels;
        test_labels.add("__name__", "boundary_large");
        test_labels.add("test", "phase1");
        test_labels.add("pool_test", "true");
        test_labels.add("size", "large");
        
        std::cout << "Iteration " << i << ": " << test_labels.to_string() 
                  << " -> ID: " << calculate_series_id(test_labels) << std::endl;
    }
    
    // Test 3: Check if std::map iteration order is consistent
    std::cout << "\nTest 3: std::map iteration order consistency\n";
    std::cout << "--------------------------------------------\n";
    
    for (int i = 0; i < 3; ++i) {
        std::map<std::string, std::string> test_map;
        test_map["__name__"] = "boundary_large";
        test_map["test"] = "phase1";
        test_map["pool_test"] = "true";
        test_map["size"] = "large";
        
        std::cout << "Map iteration " << i << ": ";
        for (const auto& [key, value] : test_map) {
            std::cout << key << "=" << value << " ";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n🎯 CONCLUSION:\n";
    std::cout << "If the string representations are different, that's the root cause!\n";
    std::cout << "The fix is to sort the labels before creating the string representation.\n";
    
    return 0;
}
