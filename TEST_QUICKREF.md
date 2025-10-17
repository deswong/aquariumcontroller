# Quick Test Reference

## Run Tests

```bash
# All tests (native - fast!)
pio test -e native

# All tests (on ESP32)
pio test -e esp32dev

# Specific test file
pio test -e native -f test_main

# With verbose output
pio test -e native -v
```

## Test Files

| File | Focus | Test Count |
|------|-------|------------|
| `test_main.cpp` | Unit tests for components | 30+ |
| `test_mocks.cpp` | Hardware mocks & storage | 15+ |
| `test_integration.cpp` | Complete workflows | 12+ |

## Common Assertions

```cpp
TEST_ASSERT_EQUAL_FLOAT(expected, actual);
TEST_ASSERT_EQUAL_INT(expected, actual);
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);
TEST_ASSERT_FLOAT_WITHIN(tolerance, expected, actual);
TEST_ASSERT_LESS_THAN(threshold, actual);
```

## Test Template

```cpp
void test_my_feature() {
    // Arrange
    float input = 25.0;
    
    // Act
    float result = myFunction(input);
    
    // Assert
    TEST_ASSERT_EQUAL_FLOAT(50.0, result);
}

// Add to runner
RUN_TEST(test_my_feature);
```

## Coverage

✅ PID controllers  
✅ Sensors (logic)  
✅ Safety systems  
✅ Configuration  
✅ Calibration  
✅ Data flow  

## See Also

- Full guide: [TESTING.md](TESTING.md)
- Summary: [TEST_SUMMARY.md](TEST_SUMMARY.md)
