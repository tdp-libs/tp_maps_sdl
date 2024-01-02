TARGET = tp_maps_sdl
TEMPLATE = lib

DEFINES += TP_MAPS_SDL_LIBRARY

SOURCES += src/Map.cpp
HEADERS += inc/tp_maps_sdl/Map.h

SOURCES += src/Vulkan.cpp
HEADERS += inc/tp_maps_sdl/Vulkan.h

SOURCES += src/Globals.cpp
HEADERS += inc/tp_maps_sdl/Globals.h
