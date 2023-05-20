add_executable(test_vector test_vector.cpp)
target_link_libraries(test_vector utils datastr GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
