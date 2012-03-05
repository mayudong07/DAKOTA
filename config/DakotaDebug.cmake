# CMake options for DAKOTA debug builds

# Use debug build options (-g)
set(CMAKE_BUILD_TYPE "DEBUG" CACHE STRING "Type of build")

# Use static libraries and don't use shared
set(BUILD_STATIC_LIBS TRUE  CACHE BOOL "Build static libs?")
set(BUILD_SHARED_LIBS FALSE CACHE BOOL "Build shared libs?")

# Force bounds checking in Teuchos
set(Teuchos_ENABLE_ABC TRUE CACHE BOOL "Enable bounds checking?")
