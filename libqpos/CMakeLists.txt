aux_source_directory(. SRC_LIST)

file(GLOB HEADERS "*.h")

add_library(qpos ${SRC_LIST} ${HEADERS})

target_include_directories(qpos PRIVATE ..)
target_include_directories(qpos PRIVATE ../evmjit/include)
target_include_directories(qpos PRIVATE .)
target_include_directories(qpos SYSTEM PUBLIC ${Boost_INCLUDE_DIR})
target_include_directories(qpos PRIVATE "${UTILS_INCLUDE_DIR}")

target_link_libraries(qpos ${Dev_DEVCORE_LIBRARIES})
target_link_libraries(qpos ${Dev_DEVCRYPTO_LIBRARIES})
target_link_libraries(qpos ${Eth_ETHCORE_LIBRARIES})
target_link_libraries(qpos ${Eth_ETHEREUM_LIBRARIES})

target_link_libraries(qpos PUBLIC ethereum ethcore)

install(TARGETS qpos RUNTIME DESTINATION bin ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)

