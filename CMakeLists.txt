cmake_minimum_required(VERSION 3.10)

project(pawnraknet)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

include(AddSAMPPlugin)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(lib/omp-sdk)

add_samp_plugin(${PROJECT_NAME}
  plugin.def

  src/Pawn.RakNet.inc
  src/main.h
  src/main.cc
  src/plugin.h
  src/plugin.cc
  src/natives.cpp
  src/config.h
  src/config.cc
  src/bitstream_pool.h
  src/bitstream_pool.cc
  src/script.h
  src/script.cc

  lib/RakNet/bitstream.hpp
  lib/RakNet/bitstream.cpp
  lib/RakNet/packet.hpp
  lib/RakNet/Encoding/huffman_tree_node.hpp
  lib/RakNet/Encoding/huffman_tree.hpp
  lib/RakNet/Encoding/huffman_tree.cpp
  lib/RakNet/Encoding/str_compress.hpp
  lib/RakNet/Encoding/str_compress.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE lib lib/pawn-natives lib/samp-plugin-sdk)

if(UNIX AND NOT APPLE)
    target_include_directories(${PROJECT_NAME} PRIVATE
        lib/linux
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE OMP-SDK)

# Override AddSAMPPlugin's hardcoded flags: PIC required for 64-bit shared libs
if(CMAKE_COMPILER_IS_GNUCC)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_STDINT_H)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_FLAGS "-m32 -O3 -w")
    set_property(TARGET ${PROJECT_NAME} PROPERTY LINK_FLAGS "-m32 -O3 -static-libgcc -static-libstdc++")
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE On)
  else()
    set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_FLAGS "-O3 -w")
    set_property(TARGET ${PROJECT_NAME} PROPERTY LINK_FLAGS "-O3 -static-libgcc -static-libstdc++")
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE On)
  endif()
endif()
