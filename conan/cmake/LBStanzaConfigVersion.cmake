# This is a basic version file for the Config-mode of find_package().
# It is used by write_basic_package_version_file() as input file for configure_file()
# to create a version-file which can be installed along a config.cmake file.
#
# The created file sets PACKAGE_VERSION_EXACT if the current version string and
# the requested version string are exactly the same and it sets
# PACKAGE_VERSION_COMPATIBLE if the current version is >= requested version,
# but only if the requested major version is the same as the current one.
# The variable CVF_VERSION must be set before calling configure_file().

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

set(PACKAGE_VERSION ${LBSTANZA_VERSION_STRING})

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()

  if("1.0.0" MATCHES "^([0-9]+)\\.")
    set(CVF_VERSION_MAJOR "${CMAKE_MATCH_1}")
  else()
    set(CVF_VERSION_MAJOR "1.0.0")
  endif()

  if(PACKAGE_FIND_VERSION_MAJOR STREQUAL CVF_VERSION_MAJOR)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
  else()
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
  endif()

  if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
      set(PACKAGE_VERSION_EXACT TRUE)
  endif()
endif()

