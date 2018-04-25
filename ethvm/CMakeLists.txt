add_executable(ethvm main.cpp)

target_link_libraries(ethvm PRIVATE ethereum evm ethashseal devcore Boost::program_options web3jsonrpc)

target_include_directories(ethvm PRIVATE ../utils)

install(TARGETS ethvm DESTINATION bin OPTIONAL)
