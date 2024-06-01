
#.rst:
# LBStanzaConfig
# --------
#
# Find stanza
#
# This module looks for stanza.  This module defines the following values:
#
# ::
#
#   LBSTANZA_EXECUTABLE: the full path to the stanza compiler.
#   LBSTANZA_FOUND: True if stanza has been found.

# Find the executable
find_program(
  LBSTANZA_EXECUTABLE
  stanza stanza.exe
  NAMES_PER_DIR
  PATH_SUFFIXES ".."
  REQUIRED
  #NO_DEFAULT_PATH
  NO_PACKAGE_ROOT_PATH
  NO_CMAKE_PATH
  NO_CMAKE_ENVIRONMENT_PATH
  #NO_SYSTEM_ENVIRONMENT_PATH
  NO_CMAKE_SYSTEM_PATH
  NO_CMAKE_INSTALL_PREFIX
  NO_CMAKE_FIND_ROOT_PATH
  )

# Get the version
if(LBSTANZA_EXECUTABLE)
  execute_process(COMMAND ${LBSTANZA_EXECUTABLE} version -terse
                  OUTPUT_VARIABLE LBSTANZA_VERSION_STRING
                  ERROR_QUIET
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

mark_as_advanced(LBSTANZA_EXECUTABLE)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LBStanza
  REQUIRED_VARS LBSTANZA_EXECUTABLE
  VERSION_VAR LBSTANZA_VERSION_STRING
  )

