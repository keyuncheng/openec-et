# minimum version of cmake
cmake_minimum_required (VERSION 3.1)

# project name
project (openec_exe)

add_subdirectory(common)
add_subdirectory(ec)
add_subdirectory(fs)
add_subdirectory(protocol)
add_subdirectory(util)


########### Executables ##############

aux_source_directory(. DIR_SRCS)

# OpenEC
add_executable(OECCoordinator OECCoordinator.cc)
add_executable(OECAgent OECAgent.cc)
add_executable(OECClient OECClient.cc)

# RS Code
add_executable(RSConvTest RSConvTest.cc)

# Azure LRC
add_executable(AzureLRCTest AzureLRCTest.cc)

# Hitchhiker
add_executable(HHXORPlusTest HHXORPlusTest.cc)

# HashTag EC
add_executable(HTECTest HTECTest.cc)

# Elastic Transformation
add_executable(ETRSConvTest ETRSConvTest.cc)
add_executable(ETAzureLRCTest ETAzureLRCTest.cc)
add_executable(ETHHXORPlusTest ETHHXORPlusTest.cc)

# # HDFS Client Test
# if (${FS_TYPE} MATCHES "HDFS")
#   add_executable(HDFSClientSeekTest HDFSClientSeekTest.cc)
# endif(${FS_TYPE} MATCHES "HDFS")



########### Link Libraries ##############

# OpenEC
target_link_libraries(OECCoordinator common pthread fs)
target_link_libraries(OECAgent common pthread fs)
target_link_libraries(OECClient common pthread)

# RS Code
target_link_libraries(RSConvTest common ec)

# Azure LRC
target_link_libraries(AzureLRCTest common ec)

# Hitchhiker
target_link_libraries(HHXORPlusTest common ec)

# HashTag EC
target_link_libraries(HTECTest common ec)

# Elastic Transformation
target_link_libraries(ETRSConvTest common ec)
target_link_libraries(ETAzureLRCTest common ec)
target_link_libraries(ETHHXORPlusTest common ec)

# # HDFS Client Test
# if (${FS_TYPE} MATCHES "HDFS")
#   target_link_libraries(HDFSClientSeekTest common fs)
# endif(${FS_TYPE} MATCHES "HDFS")

