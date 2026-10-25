# FindSDL2_mixer.cmake
find_path(SDL2_MIXER_INCLUDE_DIR
    NAMES SDL_mixer.h
    PATH_SUFFIXES SDL2
)

find_library(SDL2_MIXER_LIBRARY
    NAMES SDL2_mixer
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_mixer DEFAULT_MSG
    SDL2_MIXER_LIBRARY SDL2_MIXER_INCLUDE_DIR)

if(SDL2_MIXER_FOUND)
    set(SDL2_MIXER_LIBRARIES    ${SDL2_MIXER_LIBRARY})
    set(SDL2_MIXER_INCLUDE_DIRS ${SDL2_MIXER_INCLUDE_DIR})
endif()

mark_as_advanced(SDL2_MIXER_INCLUDE_DIR SDL2_MIXER_LIBRARY)
