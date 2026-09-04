cmake_minimum_required(VERSION 4.1.1 FATAL_ERROR)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/import-std.cmake)
set(CMAKE_CXX_EXTENSIONS OFF)

# The widget set that sits on skiff: ready-made controls a screen is built
# out of, themed rather than subclassed. Its own project rather than a second
# target inside skiff, because it depends on skiff and skiff does not depend
# on it, and that is easier to keep true when they cannot see each other's
# source.
#
# Every widget is its own module -- skiff.widgets.textbox, .button, .tabbar --
# so a screen imports the ones it uses. skiff.widgets is the umbrella for a
# screen that wants all of them.

# Where skiff comes from, and everything skiff needs after it.
#
# cmake-everywhere answers find_package: from the machine when the machine
# has the library, and from the revision named below when it does not. Named
# before project(), because that is the only place a dependency provider may
# be installed from -- and only when nothing is answering already, which is
# the case where this is being built inside somebody else's build and that
# build brought its own.
if(NOT COMMAND cme_declare_port AND NOT CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
  set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/get_cme.cmake)
endif()

project(
  skiff-widgets
  VERSION 0.1
  LANGUAGES CXX
)

# CMAKE_PROJECT_TOP_LEVEL_INCLUDES is read by the top-level project() call
# and by no other, so a build configured from a subdirectory of this has no
# provider and no sign of it: the declaration below is read by nobody and
# find_package(skiff) falls through to whatever the machine has, which for a
# library of one's own is usually a copy from a month ago.
if(NOT COMMAND cme_provider)
  message(FATAL_ERROR
    "cmake-everywhere is not installed in this build, so nothing answers "
    "find_package(skiff). CMAKE_PROJECT_TOP_LEVEL_INCLUDES is only read by "
    "the top-level project() call: configure this directory, and not a "
    "subdirectory of it.")
endif()

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed.")
endif()

# Where skiff comes from, and nothing else about it.
#
# This is the half of a port declaration that belongs to whoever chose the
# library: a repository and a revision. The other half -- what skiff needs,
# what it produces, what it is licensed under -- is in skiff's own tree and
# is read from there once it has been fetched, so it is not repeated here and
# cannot go stale here.
if(COMMAND cme_declare_port AND NOT TARGET skiff::skiff)
  cme_declare_port(
    NAME skiff
    PROVIDES skiff
    GITHUB_REPOSITORY j4niwzis/skiff
    GIT_TAG 7bcd3b24b04b053e5cf212597f036d1601f50f62
  )
endif()

# One call, answered by whatever is right: a target this build already has,
# a copy installed on the machine, or the revision named above.
if(NOT TARGET skiff::skiff)
  find_package(skiff REQUIRED)
endif()

# Globbed, so that adding a widget is adding a file.
file(GLOB skiff_widget_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -pthread -msimd128")
endif()

add_library(${PROJECT_NAME} STATIC ${skiff_widget_sources})
add_library(skiff::widgets ALIAS ${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
  FILE_SET CXX_MODULES FILES ${skiff_widget_sources})

# The name this is linked by, in a build that embeds it and in one that
# finds it installed. The alias skiff::widgets is a build-tree name and is
# not exported; install(EXPORT NAMESPACE skiff::) exports the target's own
# name, so an installed copy offered skiff::skiff-widgets and a consumer
# written against the alias stopped at "links to skiff::widgets, but the
# target was not found". EXPORT_NAME makes the two the same word.
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME widgets)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
target_link_libraries(${PROJECT_NAME} PUBLIC skiff::skiff)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions -pthread -msimd128)
endif()

# ---- What this library is, said by the library ----
if(COMMAND cme_declare_port)
  cme_declare_port(
    NAME skiff-widgets
    PROVIDES skiff-widgets skiff_widgets
    VERSION ${PROJECT_VERSION}
    GITHUB_REPOSITORY j4niwzis/skiff-widgets
    GIT_TAG main
    LICENSE AGPL-3.0-only
    DEPENDS skiff
    TARGETS skiff::widgets
  )
endif()

# ---- Installing it ----
# Installing is what a build of this library does; a build that embeds it
# does not.
#
# Inside another build its dependencies are that build's targets -- Skia
# comes from a port, as skia_skia -- and exporting a target that links one of
# those is an error: "requires target skia_skia that is not in any export
# set". Which is right. What would be installed there is a library whose
# dependencies exist only in the build directory it came from.
option(SKIFF_WIDGETS_INSTALL "Install skiff-widgets and its CMake package"
       ${PROJECT_IS_TOP_LEVEL})
if(SKIFF_WIDGETS_INSTALL)
  include(GNUInstallDirs)
  install(TARGETS ${PROJECT_NAME}
    EXPORT skiffWidgetsTargets
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_LIBDIR}/skiff-widgets/modules
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(EXPORT skiffWidgetsTargets
    NAMESPACE skiff::
    CXX_MODULES_DIRECTORY modules
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/skiff-widgets)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/skiff-widgetsConfig.cmake"
    "include(CMakeFindDependencyMacro)\n"
    "find_dependency(skiff)\n"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/skiffWidgetsTargets.cmake\")\n")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/skiff-widgetsConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/skiff-widgets)
endif()
