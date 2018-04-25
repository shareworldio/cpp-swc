add_definitions(-Ddev=dev_SOLIDITY)

set(SOLIDITY_SOURCE_DIR ${CMAKE_SOURCE_DIR}/solidity)
set(SOLIDITY_INCLUDE_DIR ${SOLIDITY_SOURCE_DIR} ${SOLIDITY_SOURCE_DIR}/build/include ${SOLIDITY_SOURCE_DIR}/build/include/solidity)

file(GLOB_RECURSE SOLIDITY_SOL_sources "${SOLIDITY_SOURCE_DIR}/*.cpp") 
file(GLOB_RECURSE SOLIDITY_SOL_headers "${SOLIDITY_SOURCE_DIR}/*.h")

find_package(Z3 QUIET) 
if (${Z3_FOUND}) 
	include_directories(${Z3_INCLUDE_DIR})
	add_definitions(-DHAVE_Z3)
	message("Z3 SMT solver FOUND.") 
else() 
	message("Z3 SMT solver NOT found.")
	list(REMOVE_ITEM SOLIDITY_SOL_sources "${SOLIDITY_SOURCE_DIR}/libsolidity/formal/Z3Interface.cpp") 
endif()

add_library(solidity ${SOLIDITY_SOL_sources} ${SOLIDITY_SOL_headers}) 

get_target_property(SOL_INCLUDES solidity INCLUDE_DIRECTORIES)
list(REMOVE_ITEM SOL_INCLUDES ${CMAKE_SOURCE_DIR}) 
message("SOL_INCLUDES=${SOL_INCLUDES}") 
set_target_properties(solidity PROPERTIES INCLUDE_DIRECTORIES ${SOL_INCLUDES})

target_include_directories(solidity PRIVATE ${SOLIDITY_INCLUDE_DIR}) 
target_include_directories(solidity SYSTEM PUBLIC ${Boost_INCLUDE_DIR})

file(GLOB SRC_LIST "solidity.cpp")
file(GLOB HEADERS "*.h")

add_library(contract ${SRC_LIST} ${HEADERS})

target_include_directories(contract PRIVATE ${SOLIDITY_INCLUDE_DIR})
target_include_directories(contract PRIVATE ${Boost_INCLUDE_DIR})

target_link_libraries(contract solidity)

target_link_libraries(contract ${Boost_PROGRAM_OPTIONS_LIBRARIES}) 
target_link_libraries(contract ${Boost_REGEX_LIBRARIES})
target_link_libraries(contract ${Boost_FILESYSTEM_LIBRARIES})
target_link_libraries(contract ${Boost_SYSTEM_LIBRARIES})
target_link_libraries(contract jsoncpp_lib_static)
if (${Z3_FOUND}) 
	target_link_libraries(contract z3)
endif()

