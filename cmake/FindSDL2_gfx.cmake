# FindSDL2_gfx.cmake
find_path(SDL2_GFX_INCLUDE_DIR
    NAMES SDL2_gfxPrimitives.h
    PATH_SUFFIXES SDL2
)

find_library(SDL2_GFX_LIBRARY
    NAMES SDL2_gfx
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_gfx DEFAULT_MSG
    SDL2_GFX_LIBRARY SDL2_GFX_INCLUDE_DIR)

if(SDL2_GFX_FOUND)
    set(SDL2_GFX_LIBRARIES    ${SDL2_GFX_LIBRARY})
    set(SDL2_GFX_INCLUDE_DIRS ${SDL2_GFX_INCLUDE_DIR})
endif()

mark_as_advanced(SDL2_GFX_INCLUDE_DIR SDL2_GFX_LIBRARY)
