add_executable(pty_example pty_example.cpp)
target_link_libraries(pty_example terminal Threads::Threads)

add_executable(termbench termbench.cpp)
