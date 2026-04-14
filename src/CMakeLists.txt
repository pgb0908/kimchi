

find_package(proxygen REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE proxygen::proxygenlib proxygen::proxygenhttpserver)