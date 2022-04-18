# minimum version of cmake
cmake_minimum_required (VERSION 3.1)

# project name
project (openec_exe)

add_subdirectory(common)
add_subdirectory(ec)
add_subdirectory(fs)
add_subdirectory(protocol)
add_subdirectory(util)

aux_source_directory(. DIR_SRCS)
add_executable(OECCoordinator OECCoordinator.cc)
add_executable(OECAgent OECAgent.cc)
add_executable(OECClient OECClient.cc)
add_executable(ECDAGTest ECDAGTest.cc)
add_executable(CodeTest CodeTest.cc)

# RS Code
add_executable(RSConvTest RSConvTest.cc)

# Hitchhiker
add_executable(HHXORTest HHXORTest.cc)
add_executable(HHXORPlusTest HHXORPlusTest.cc)
add_executable(HHNonXORTest HHNonXORTest.cc)

# Elastic Transformation
add_executable(ETRSConvTest ETRSConvTest.cc)


# if (${FS_TYPE} MATCHES "HDFS")
#   add_executable(HDFSClient HDFSClient.cc)
# endif(${FS_TYPE} MATCHES "HDFS")

target_link_libraries(OECCoordinator common pthread fs)
target_link_libraries(OECAgent common pthread fs)
target_link_libraries(OECClient common pthread)
target_link_libraries(ECDAGTest common ec)
target_link_libraries(CodeTest common ec)

# RS Code
target_link_libraries(RSConvTest common ec)

# Hitchhiker
target_link_libraries(HHXORTest common ec)
target_link_libraries(HHXORPlusTest common ec)
target_link_libraries(HHNonXORTest common ec)

# Elastic Transformation
target_link_libraries(ETRSConvTest common ec)


# if (${FS_TYPE} MATCHES "HDFS")
#   target_link_libraries(HDFSClient common fs)
# endif(${FS_TYPE} MATCHES "HDFS")

