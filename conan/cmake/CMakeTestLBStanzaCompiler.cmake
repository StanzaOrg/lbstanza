# CMakeTestLBStanzaCompiler.cmake
# Verifies that the selected L.B. Stanza compiler can produce a working program.

include(CMakeTestCompilerCommon)

if(NOT CMAKE_LBStanza_COMPILER_WORKS)
  PrintTestCompilerStatus("LBStanza" " -- Testing that stanza compiler works...")
  file(WRITE
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/timon.stanza
    "defpackage helloworld :\n  import core\nprintln(\"Hello Timon\")\n"
    )
  try_compile(
    CMAKE_LBStanza_COMPILER_WORKS ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/timon.stanza
    OUTPUT_VARIABLE __CMAKE_LBStanza_COMPILER_OUTPUT
  )
endif ()

if(NOT CMAKE_LBStanza_COMPILER_WORKS)
  PrintTestCompilerStatus("LBStanza" " -- broken")
  file(APPEND
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
    "Determining if the LBStanza compiler works failed with "
    "the following output:\n${__CMAKE_LBStanza_COMPILER_OUTPUT}\n\n")
  message(FATAL_ERROR
    "The LBStanza compiler \"${CMAKE_LBStanza_COMPILER}\" "
    "is not able to compile a simple test program.\nIt fails "
    "with the following output:\n ${__CMAKE_LBStanza_COMPILER_OUTPUT}\n\n"
    "CMake will not be able to correctly generate this project.")
else()
  PrintTestCompilerStatus("LBStanza" " -- works")
  file(APPEND
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
    "Determining if the LBStanza compiler works passed with "
    "the following output:\n${__CMAKE_LBStanza_COMPILER_OUTPUT}\n\n"
  )
endif()
