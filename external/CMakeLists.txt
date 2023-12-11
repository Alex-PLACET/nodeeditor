if(BUILD_TESTING)
  find_package(Catch2 QUIET)

  if(NOT Catch2_FOUND)
    add_subdirectory(Catch2)
  endif()
endif()

# FunctionTraits
add_library(FunctionTraits INTERFACE)
target_include_directories(FunctionTraits INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/FunctionTraits/>
  $<INSTALL_INTERFACE:include/FunctionTraits>
)
