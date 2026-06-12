# CMake Windows build dependencies module

include_guard(GLOBAL)

include(buildspec_common)

# _check_dependencies_windows: Set up Windows slice for _check_dependencies
function(_check_dependencies_windows)
  # CMAKE_VS_PLATFORM_NAME is only set by Visual Studio generators; default to x64 for Ninja
  if(CMAKE_VS_PLATFORM_NAME)
    set(arch ${CMAKE_VS_PLATFORM_NAME})
  else()
    set(arch "x64")
  endif()
  set(platform windows-${arch})

  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  set(prebuilt_filename "windows-deps-VERSION-ARCH-REVISION.zip")
  set(prebuilt_destination "obs-deps-VERSION-ARCH")
  set(qt6_filename "windows-deps-qt6-VERSION-ARCH-REVISION.zip")
  set(qt6_destination "obs-deps-qt6-VERSION-ARCH")
  set(obs-studio_filename "VERSION.zip")
  set(obs-studio_destination "obs-studio-VERSION")
  set(dependencies_list prebuilt qt6 obs-studio)

  _check_dependencies()
  # Propagate the cache-updated CMAKE_PREFIX_PATH back to the caller's scope.
  # On Windows, vcpkg's toolchain sets CMAKE_PREFIX_PATH as a normal variable
  # before CMakeLists.txt runs, which shadows the CACHE update made inside
  # _check_dependencies(). Reading $CACHE{} bypasses the stale normal variable.
  set(CMAKE_PREFIX_PATH "$CACHE{CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()

_check_dependencies_windows()
