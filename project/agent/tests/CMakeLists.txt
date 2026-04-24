add_executable(pi_guard_foundation_tests
    foundation/thread_safe_queue_test.cpp
)

target_link_libraries(pi_guard_foundation_tests
    PRIVATE
        pi_guard_foundation
)

add_test(NAME foundation.thread_safe_queue COMMAND pi_guard_foundation_tests)
