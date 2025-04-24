# Nova Language Test Suite

This directory contains test cases for the Nova programming language.

## Test Files

The test suite includes the following categories of tests:

1. **Basic Language Features**
   - `01_variables.nova` - Test variable declarations and assignments
   - `02_operators.nova` - Test arithmetic, comparison, and logical operators
   - `03_conditionals.nova` - Test conditional statements
   - `04_loops.nova` - Test loop constructs
   - `05_functions.nova` - Test function definition and calling

2. **Advanced Language Features**
   - `06_scope.nova` - Test variable scopes (global, local)
   - `07_data_structures.nova` - Test structs, lists, and other data structures
   - `08_type_inference.nova` - Test type inference system
   - `09_error_handling.nova` - Test error handling mechanisms
   - `10_math_operations.nova` - Test complex mathematical operations

3. **Object-Oriented Features**
   - `11_classes.nova` - Test class definitions and usage
   - `12_inheritance.nova` - Test inheritance and polymorphism
   - `13_encapsulation.nova` - Test encapsulation and access modifiers

## How to Run Tests

### Running All Tests

To run all tests, use:

```bash
cd build
cmake ..
make run_all_tests
```

### Running Individual Tests

To run a specific test, use:

```bash
cd build
cmake ..
make run_01_variables  # Replace with the test name you want to run
```

### Capturing Expected Output

To capture the current output of tests as the expected output:

```bash
cd build
cmake ..
make capture_expected_output
```

This will create files in the `expected/` directory, which will be used for verification in subsequent test runs.

## Adding New Tests

To add a new test case:

1. Create a new `.nova` file in the test directory
2. Follow the naming convention: `XX_description.nova`
3. Run `make capture_expected_output` to generate the expected output

## Verification

The test system compares the output of each test with the expected output stored in the `expected/` directory. If they match, the test passes; otherwise, it fails. 