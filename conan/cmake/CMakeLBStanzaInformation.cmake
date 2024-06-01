# CMakeLBStanzaInformation.cmake
# Provides information to CMake about how to build L.B. Stanza files.
# https://lbstanza.org

set(CMAKE_LBStanza_OUTPUT_EXTENSION .s)

# set CMAKE_LBStanza_PLATFORM
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
  string(TOLOWER ${CMAKE_SYSTEM_NAME} CMAKE_LBStanza_PLATFORM)
  string(TOUPPER "PLATFORM_${CMAKE_LBStanza_PLATFORM}" CMAKE_LBStanza_PLATFORM_DEF)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Macos")
  set(CMAKE_LBStanza_PLATFORM "os-x")
  set(CMAKE_LBStanza_PLATFORM_DEF "PLATFORM_OS_X")
else()
  message(FATAL_ERROR "LBStanza doesn't support CMAKE_SYSTEM_NAME \"${CMAKE_SYSTEM_NAME}\"")
endif()

# Stanza compile to assembly files
if (NOT CMAKE_LBStanza_COMPILE_OBJECT)
  set (CMAKE_LBStanza_COMPILE_OBJECT
    "<CMAKE_LBStanza_COMPILER> compile <SOURCE> -s <OBJECT>"
  )
endif ()

# Create a static archive incrementally for large object file counts.
# If CMAKE_LBStanza_CREATE_STATIC_LIBRARY is set it will override these.
if(NOT DEFINED CMAKE_LBStanza_ARCHIVE_CREATE)
  set(CMAKE_LBStanza_ARCHIVE_CREATE "<CMAKE_AR> qc <TARGET> <LINK_FLAGS> <OBJECTS>")
endif()
if(NOT DEFINED CMAKE_LBStanza_ARCHIVE_APPEND)
  set(CMAKE_LBStanza_ARCHIVE_APPEND "<CMAKE_AR> q <TARGET> <LINK_FLAGS> <OBJECTS>")
endif()
if(NOT DEFINED CMAKE_LBStanza_ARCHIVE_FINISH)
  set(CMAKE_LBStanza_ARCHIVE_FINISH "<CMAKE_RANLIB> <TARGET>")
endif()

# Stanza compile to executable
if (NOT CMAKE_LBStanza_LINK_EXECUTABLE)
  set (CMAKE_LBStanza_LINK_EXECUTABLE
    "cc <FLAGS> <LINK_FLAGS> <OBJECTS> -D${CMAKE_LBStanza_PLATFORM_DEF} -I$$STANZA_CONFIG/include $$STANZA_CONFIG/runtime/driver.c -lm -o <TARGET> <LINK_LIBRARIES>"
  )
endif ()

#gcc -std=gnu99 -c core/sha256.c -O3 -o build/sha256.o -I include
#gcc -std=gnu99 -c compiler/cvm.c -O3 -o build/cvm.o -I include
#gcc -c runtime/linenoise-ng/linenoise.cpp -O3 -o build/linenoise.o -I include -I runtime/linenoise-ng
#gcc -c runtime/linenoise-ng/ConvertUTF.cpp -O3 -o build/ConvertUTF.o -I runtime/linenoise-ng
#gcc -c runtime/linenoise-ng/wcwidth.cpp -O3 -o build/wcwidth.o -I runtime/linenoise-ng
#gcc -std=gnu99 core/threadedreader.c runtime/driver.c compiler/exec-alloc.c build/linenoise.o build/ConvertUTF.o build/wcwidth.o build/cvm.o build/sha256.o stanza.s -o stanza -DPLATFORM_OS_X -lm -I include -Lbin -lasmjit-os-x -lc++

set (CMAKE_LBStanza_INFORMATION_LOADED 1)




# now define the following rules:
# CMAKE_CXX_CREATE_SHARED_LIBRARY
# CMAKE_CXX_CREATE_SHARED_MODULE
# CMAKE_CXX_COMPILE_OBJECT
# CMAKE_CXX_LINK_EXECUTABLE

# variables supplied by the generator at use time
# <TARGET>
# <TARGET_BASE> the target without the suffix
# <OBJECTS>
# <OBJECT>
# <LINK_LIBRARIES>
# <FLAGS>
# <LINK_FLAGS>

# CXX compiler information
# <CMAKE_CXX_COMPILER>
# <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS>
# <CMAKE_CXX_SHARED_MODULE_CREATE_FLAGS>
# <CMAKE_CXX_LINK_FLAGS>

# Static library tools
# <CMAKE_AR>
# <CMAKE_RANLIB>


## create a shared C++ library
#if(NOT CMAKE_CXX_CREATE_SHARED_LIBRARY)
#  set(CMAKE_CXX_CREATE_SHARED_LIBRARY
#      "<CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <SONAME_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS># <LINK_LIBRARIES>")
#endif()
#
## create a c++ shared module copy the shared library rule by default
#if(NOT CMAKE_CXX_CREATE_SHARED_MODULE)
#  set(CMAKE_CXX_CREATE_SHARED_MODULE ${CMAKE_CXX_CREATE_SHARED_LIBRARY})
#endif()
#
#
## Create a static archive incrementally for large object file counts.
## If CMAKE_CXX_CREATE_STATIC_LIBRARY is set it will override these.
#if(NOT DEFINED CMAKE_CXX_ARCHIVE_CREATE)
#  set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qc <TARGET> <LINK_FLAGS> <OBJECTS>")
#endif()
#if(NOT DEFINED CMAKE_CXX_ARCHIVE_APPEND)
#  set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> q <TARGET> <LINK_FLAGS> <OBJECTS>")
#endif()
#if(NOT DEFINED CMAKE_CXX_ARCHIVE_FINISH)
#  set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> <TARGET>")
#endif()
#
## compile a C++ file into an object file
#if(NOT CMAKE_CXX_COMPILE_OBJECT)
#  set(CMAKE_CXX_COMPILE_OBJECT
#    "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT> -c <SOURCE>")
#endif()
#
#if(NOT CMAKE_CXX_LINK_EXECUTABLE)
#  set(CMAKE_CXX_LINK_EXECUTABLE
#    "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
#endif()

