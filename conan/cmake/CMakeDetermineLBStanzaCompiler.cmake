# CMakeDetermineLBStanzaCompiler.cmake
# Determines which compiler to use for L.B. Stanza files.
# https://lbstanza.org

set(CMAKE_LBStanza_COMPILER_ENV_VAR "LBSTANZA_COMPILER")
set(CMAKE_LBStanza_SOURCE_FILE_EXTENSIONS stanza)
set(CMAKE_LBStanza_OUTPUT_EXTENSION .s)

# find L.B. Stanza
find_package(LBStanza CONFIG REQUIRED)
set(
  CMAKE_LBStanza_COMPILER ${LBSTANZA_EXECUTABLE}
  )
mark_as_advanced(CMAKE_LBStanza_COMPILER)

# Configure the variables in this file for faster reloads
configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/CMakeLBStanzaCompiler.cmake.in
  ${CMAKE_PLATFORM_INFO_DIR}/CMakeLBStanzaCompiler.cmake
  @ONLY
  )
