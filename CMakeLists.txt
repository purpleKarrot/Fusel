#=============================================================================
# Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt
#=============================================================================

cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)
project(Fusel C)

list(APPEND CMAKE_MODULE_PATH "${Fusel_SOURCE_DIR}/CMake")

find_package(Karrot REQUIRED)
find_package(LibGit2 REQUIRED)

include_directories(
  ${Karrot_INCLUDE_DIRS}
  ${LibGit2_INCLUDE_DIR}
  )

add_executable(fusel
  fusel.c
  )

target_link_libraries(fusel
  ${Karrot_LIBRARIES}
  ${LibGit2_LIBRARY}
  )
