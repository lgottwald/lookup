cmake_minimum_required(VERSION 2.8)

FIND_PACKAGE(Boost COMPONENTS serialization iostreams REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cpplsq REQUIRED)
find_package(libspline REQUIRED)
find_package(libsdo REQUIRED)
find_package(OpenMP)
if(OPENMP_FOUND)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

INCLUDE_DIRECTORIES(
  ${Boost_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
  ${cpplsq_INCLUDE_DIRS}
  ${libspline_INCLUDE_DIRS}
  ${libsdo_INCLUDE_DIRS}
)

add_definitions(
  ${cpplsq_DEFINITIONS}
)

ADD_LIBRARY(lookup SHARED
        lookuplib.cpp
        )

set_target_properties( lookup PROPERTIES COMPILE_FLAGS "-std=c++11 -Wall -Wextra -pedantic-errors -Wno-unused-local-typedefs -Wno-write-strings -Wno-unused-parameter" )
TARGET_LINK_LIBRARIES( lookup ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${cpplsq_LIBRARIES} ${libsdo_LIBRARIES} )

INSTALL(TARGETS lookup LIBRARY DESTINATION lib)