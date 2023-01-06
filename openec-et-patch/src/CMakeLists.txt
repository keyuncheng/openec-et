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

# # HDFS Client Test
# if (${FS_TYPE} MATCHES "HDFS")
#   add_executable(HDFSClientSeekTest HDFSClientSeekTest.cc)
# endif(${FS_TYPE} MATCHES "HDFS")



########### Link Libraries ##############

# OpenEC
target_link_libraries(OECCoordinator common pthread fs)
target_link_libraries(OECAgent common pthread fs)
target_link_libraries(OECClient common pthread)

# # HDFS Client Test
# if (${FS_TYPE} MATCHES "HDFS")
#   target_link_libraries(HDFSClientSeekTest common fs)
# endif(${FS_TYPE} MATCHES "HDFS")

