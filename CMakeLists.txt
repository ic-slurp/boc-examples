cmake_minimum_required(VERSION 3.10.0)

project(boc-artifacts CXX)

set(VERONA_RT_ONLY_HEADER_LIBRARY ON)

add_subdirectory(verona/src/rt)

set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)
subdirlist(EXAMPLES ${EXAMPLES_DIR})

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

unset(SRC)
aux_source_directory(verona/src/rt/test/perf/dining_phil/ SRC)
add_executable(benchmark ${SRC})
target_link_libraries(benchmark verona_rt)