# Script to generate test summary from GTest XML output

# Function to parse XML and extract test results
function(parse_test_results xml_file)
    file(READ "${xml_file}" xml_content)
    
    # Extract test counts
    string(REGEX MATCH "tests=\"([0-9]+)\"" _ "${xml_content}")
    set(total_tests ${CMAKE_MATCH_1})
    
    string(REGEX MATCH "failures=\"([0-9]+)\"" _ "${xml_content}")
    set(failed_tests ${CMAKE_MATCH_1})
    
    string(REGEX MATCH "disabled=\"([0-9]+)\"" _ "${xml_content}")
    set(disabled_tests ${CMAKE_MATCH_1})
    
    string(REGEX MATCH "errors=\"([0-9]+)\"" _ "${xml_content}")
    set(error_tests ${CMAKE_MATCH_1})
    
    # Calculate passed tests
    math(EXPR passed_tests "${total_tests} - ${failed_tests} - ${disabled_tests} - ${error_tests}")
    
    # Extract test duration
    string(REGEX MATCH "time=\"([0-9.]+)\"" _ "${xml_content}")
    set(total_time ${CMAKE_MATCH_1})
    
    # Write summary
    file(APPEND "${CMAKE_BINARY_DIR}/test_results/summary.txt"
        "Test Suite: ${xml_file}\n"
        "Total Tests: ${total_tests}\n"
        "Passed: ${passed_tests}\n"
        "Failed: ${failed_tests}\n"
        "Disabled: ${disabled_tests}\n"
        "Errors: ${error_tests}\n"
        "Total Time: ${total_time}s\n\n"
    )
    
    # Extract failed test details
    string(REGEX MATCHALL "<testcase[^>]*>.*?</testcase>" testcases "${xml_content}")
    foreach(testcase ${testcases})
        if(testcase MATCHES "<failure")
            string(REGEX MATCH "name=\"([^\"]*)\"" _ "${testcase}")
            set(test_name ${CMAKE_MATCH_1})
            string(REGEX MATCH "<failure[^>]*message=\"([^\"]*)\"" _ "${testcase}")
            set(failure_message ${CMAKE_MATCH_1})
            file(APPEND "${CMAKE_BINARY_DIR}/test_results/failures.txt"
                "Failed Test: ${test_name}\n"
                "Message: ${failure_message}\n\n"
            )
        endif()
    endforeach()
endfunction()

# Initialize summary files
file(WRITE "${CMAKE_BINARY_DIR}/test_results/summary.txt" "Test Summary\n============\n\n")
file(WRITE "${CMAKE_BINARY_DIR}/test_results/failures.txt" "Test Failures\n============\n\n")

# Process each test result file
file(GLOB test_results "${CMAKE_BINARY_DIR}/test_results/*.xml")
foreach(result ${test_results})
    parse_test_results(${result})
endforeach()

# Read and display summary
file(READ "${CMAKE_BINARY_DIR}/test_results/summary.txt" summary)
message("\n${summary}")

# Check for failures
file(READ "${CMAKE_BINARY_DIR}/test_results/failures.txt" failures)
if(NOT failures STREQUAL "Test Failures\n============\n\n")
    message(WARNING "\n${failures}")
endif() 