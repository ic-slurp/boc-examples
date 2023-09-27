cmake_minimum_required(VERSION 3.18.0)

project(boc-artifacts CXX)

include(FetchContent)

FetchContent_Declare(
  verona
  GIT_REPOSITORY https://github.com/microsoft/verona-rt
  GIT_TAG        18ce6f3
  SOURCE_SUBDIR  src
)

FetchContent_MakeAvailable(verona)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)
subdirlist(EXAMPLES ${EXAMPLES_DIR})

set(CMAKE_CXX_FLAGS)

foreach(EXAMPLE ${EXAMPLES})
  unset(SRC)
  aux_source_directory(${EXAMPLES_DIR}/${EXAMPLE} SRC)
  add_executable(${EXAMPLE} ${SRC})
  if (${EXAMPLE} STREQUAL "boids")
    target_link_libraries(${EXAMPLE} sfml-graphics sfml-window sfml-system verona_rt)
  else()
    target_link_libraries(${EXAMPLE} verona_rt)
  endif()
endforeach()