cmake_minimum_required(VERSION 4.1.1 FATAL_ERROR)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/import-std.cmake)
# C++23 for every library this build makes, not only for its own targets:
# what it builds of its dependencies -- Boost.PFR as a module, splice --
# imports std, which CMake gives a target only at C++23.
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
endif()
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
  VERSION 0.1.0
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
if(COMMAND cme_declare_port AND NOT TARGET skiff::core)
  cme_declare_port(
    NAME skiff
    PROVIDES skiff
    GITHUB_REPOSITORY j4niwzis/skiff
    GIT_TAG 33602440b84b96fc0ea36321c85a990eb3550d64
    VERSION 0.1.0
  )
endif()

# One call, answered by whatever is right: a target this build already has,
# a copy installed on the machine, or the revision named above.
if(NOT TARGET skiff::core)
  find_package(skiff 0.1.0 REQUIRED)
endif()

# Globbed, so that adding a widget is adding a file.
file(GLOB skiff_widget_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
# The theme is every widget's and no backend's; the page's controls are
# skiff.web's; skiff.ui is one of two. Each is a library of its own below.
list(REMOVE_ITEM skiff_widget_sources
  "${CMAKE_CURRENT_SOURCE_DIR}/src/theme.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/web.cc")

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  # As skiff was built: SKIFF_EMSCRIPTEN_THREADS is its cache option, read
  # here so that a page built without threads links against a skiff built
  # without them.
  if(DEFINED SKIFF_EMSCRIPTEN_THREADS AND NOT SKIFF_EMSCRIPTEN_THREADS)
    set(skiff_emscripten_flags -fexceptions)
  else()
    set(skiff_emscripten_flags -fexceptions -pthread -msimd128)
  endif()
  # Only what is missing, as skiff adds it: the same flag said twice is
  # another set of flags, and another std module made for it.
  set(skiff_widgets_emscripten_missing)
  foreach(flag IN LISTS skiff_emscripten_flags)
    if(NOT " ${CMAKE_CXX_FLAGS} " MATCHES " ${flag} ")
      list(APPEND skiff_widgets_emscripten_missing ${flag})
    endif()
  endforeach()
  if(skiff_widgets_emscripten_missing)
    string(JOIN " " skiff_widgets_emscripten_flags ${skiff_widgets_emscripten_missing})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${skiff_widgets_emscripten_flags}")
  endif()
endif()

# skiff.widgets.theme: the palette and metrics every widget draws from, with
# no backend in it -- a Colour is skiff.core's word -- so the scene's
# widgets and the page's dress alike.
add_library(skiff_widgets_theme STATIC)
add_library(skiff::widgets_theme ALIAS skiff_widgets_theme)
target_sources(skiff_widgets_theme PUBLIC
  FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/theme.cc)
set_target_properties(skiff_widgets_theme PROPERTIES EXPORT_NAME widgets_theme CXX_STANDARD 23)
target_compile_features(skiff_widgets_theme PUBLIC cxx_std_23)
target_link_libraries(skiff_widgets_theme PUBLIC skiff::core)
set(skiff_widgets_built skiff_widgets_theme)

# The widgets on the scene, where the scene is built.
if(TARGET skiff::skiff)
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
target_link_libraries(${PROJECT_NAME} PUBLIC skiff::skiff skiff::widgets_theme)

list(APPEND skiff_widgets_built ${PROJECT_NAME})
endif()

# skiff.widgets.web: the same controls on the page, where skiff.web is built.
if(TARGET skiff::web)
add_library(skiff_widgets_web STATIC)
add_library(skiff::widgets_web ALIAS skiff_widgets_web)
target_sources(skiff_widgets_web PUBLIC
  FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/web.cc)
set_target_properties(skiff_widgets_web PROPERTIES EXPORT_NAME widgets_web CXX_STANDARD 23)
target_compile_features(skiff_widgets_web PUBLIC cxx_std_23)
target_link_libraries(skiff_widgets_web PUBLIC skiff::web skiff::widgets_theme)
list(APPEND skiff_widgets_built skiff_widgets_web)
endif()

# skiff.ui: the one vocabulary a program writes its UI in, for both
# backends at once (src/ui/ui.cc): a tree said in it is mounted on the page
# (skiff.web and its widgets) or on the scene (skiff.compose and the
# widgets), in one program if it likes. Built where either backend is; it
# mounts on those that are (SKIFF_UI_ON_PAGE, SKIFF_UI_ON_SCENE).
if(TARGET skiff::widgets OR TARGET skiff::widgets_web)
  add_library(skiff_ui STATIC)
  target_sources(skiff_ui PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/ui.cc)
  target_link_libraries(skiff_ui PUBLIC skiff::widgets_theme)
  if(TARGET skiff::widgets)
    target_link_libraries(skiff_ui PUBLIC skiff::widgets)
    target_compile_definitions(skiff_ui PUBLIC SKIFF_UI_ON_SCENE=1)
  endif()
  if(TARGET skiff::widgets_web)
    target_link_libraries(skiff_ui PUBLIC skiff::widgets_web)
    target_compile_definitions(skiff_ui PUBLIC SKIFF_UI_ON_PAGE=1)
  endif()
  # SKIFF_NAMED names a view over skiff.ui's Named: made the backend's
  # named view where it is mounted.
  target_compile_definitions(skiff_ui PUBLIC SKIFF_NAMED_BASE=::skiff::ui::Named)
  add_library(skiff::ui ALIAS skiff_ui)
  set_target_properties(skiff_ui PROPERTIES EXPORT_NAME ui CXX_STANDARD 23)
  target_compile_features(skiff_ui PUBLIC cxx_std_23)
  list(APPEND skiff_widgets_built skiff_ui)
endif()

# skiff.ui.backend: one backend's own names under skiff::ui -- the scene's
# (src/ui/scene.cc) or the page's (src/ui/web.cc), picked here -- for a
# program built for one backend at a time.
set(SKIFF_UI_BACKEND "" CACHE STRING "What answers skiff.ui.backend: scene or web; empty for the scene where it is built, else the page")
if(SKIFF_UI_BACKEND STREQUAL "")
  if(TARGET skiff::widgets)
    set(skiff_ui_backend scene)
  else()
    set(skiff_ui_backend web)
  endif()
else()
  set(skiff_ui_backend ${SKIFF_UI_BACKEND})
endif()
if(skiff_ui_backend STREQUAL "scene" AND TARGET skiff::widgets)
  add_library(skiff_ui_backend STATIC)
  target_sources(skiff_ui_backend PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/scene.cc)
  target_link_libraries(skiff_ui_backend PUBLIC skiff::widgets)
elseif(skiff_ui_backend STREQUAL "web" AND TARGET skiff::widgets_web)
  add_library(skiff_ui_backend STATIC)
  target_sources(skiff_ui_backend PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/web.cc)
  target_link_libraries(skiff_ui_backend PUBLIC skiff::widgets_web)
  # SKIFF_NAMED names a view over the page's Named here.
  target_compile_definitions(skiff_ui_backend PUBLIC SKIFF_NAMED_BASE=::skiff::web::Named)
else()
  message(FATAL_ERROR "skiff.ui.backend on '${skiff_ui_backend}', but that backend is not built here")
endif()
add_library(skiff::ui_backend ALIAS skiff_ui_backend)
set_target_properties(skiff_ui_backend PROPERTIES EXPORT_NAME ui_backend CXX_STANDARD 23)
target_compile_features(skiff_ui_backend PUBLIC cxx_std_23)
list(APPEND skiff_widgets_built skiff_ui_backend)

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
    TARGETS skiff::ui
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
  install(TARGETS ${skiff_widgets_built}
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
