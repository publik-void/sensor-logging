add_library(_433D _433D.c)

find_package(Threads REQUIRED)
target_link_libraries(_433D Threads::Threads)
target_link_libraries(_433D pigpiod_if2)
