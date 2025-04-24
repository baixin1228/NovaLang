cmake_minimum_required(VERSION 3.10)

# Enable testing
enable_testing()

# Define a function to run the nova compiler with output verification
function(add_nova_test TEST_FILE)
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    
    # Run the Nova program and capture output
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${CMAKE_COMMAND} -E env NOVAHOME=${CMAKE_BINARY_DIR} 
                ${CMAKE_BINARY_DIR}/nova ${TEST_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # Set test properties for output verification (if a verification file exists)
    set(VERIFICATION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/expected/${TEST_NAME}.txt")
    if(EXISTS ${VERIFICATION_FILE})
        set_tests_properties(${TEST_NAME} PROPERTIES
            PASS_REGULAR_EXPRESSION "^$(cat ${VERIFICATION_FILE})$"
        )
    endif()
    
    # Add custom target to run this test individually
    add_custom_target(run_${TEST_NAME}
        COMMAND ${CMAKE_COMMAND} -E env NOVAHOME=${CMAKE_BINARY_DIR}
                ${CMAKE_BINARY_DIR}/nova ${TEST_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running ${TEST_NAME} test..."
    )
endfunction()

# Create expected output directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/expected)

# Find all Nova test files
file(GLOB NOVA_TEST_FILES "*.nova")

# Add tests for each Nova file
foreach(TEST_FILE ${NOVA_TEST_FILES})
    add_nova_test(${TEST_FILE})
endforeach()

# Add a custom target to run all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    COMMENT "Running all Nova tests..."
)

# Add a custom target to capture expected output for all tests
add_custom_target(capture_expected_output
    COMMENT "Capturing expected output for all tests..."
)

# For each test, add a command to run the test and capture its output
foreach(TEST_FILE ${NOVA_TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_custom_command(
        TARGET capture_expected_output
        COMMAND ${CMAKE_COMMAND} -E env NOVAHOME=${CMAKE_BINARY_DIR}
                ${CMAKE_BINARY_DIR}/nova ${TEST_FILE} > 
                ${CMAKE_CURRENT_SOURCE_DIR}/expected/${TEST_NAME}.txt
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Capturing output for ${TEST_NAME}..."
    )
endforeach()

# Print a message
message(STATUS "Configured ${CMAKE_CURRENT_SOURCE_DIR} tests with output verification")
message(STATUS "Use 'make run_TESTNAME' to run a specific test")
message(STATUS "Use 'make run_all_tests' to run all tests")
message(STATUS "Use 'make capture_expected_output' to capture expected output for all tests") 