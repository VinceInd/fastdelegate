add_executable(DelegateTests 
    DelegateTests.cpp)
target_include_directories(DelegateTests PUBLIC ${CONAN_INCLUDE_DIRS_GTEST})
target_link_libraries(DelegateTests gtest_main gtest)
