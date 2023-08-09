add_executable (prio-test test.c prio_pool.cpp)
target_link_libraries (prio-test PkgConfig::abt)
