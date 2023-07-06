add_library(DHTXXD DHTXXD.c)

find_package(Threads REQUIRED)
target_link_libraries(DHTXXD Threads::Threads)
target_link_libraries(DHTXXD pigpiod_if2)
