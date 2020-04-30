
if(ANDROID)
  get_filename_component(SDL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../android-sdl2" ABSOLUTE)
  list(APPEND TP_LIBRARIES "-L${SDL_ROOT}/release/jni/${ANDROID_ABI}/")
  list(APPEND TP_INCLUDEPATHS "${SDL_ROOT}/include")
else()
endif()

list(APPEND TP_LIBRARIES "-lSDL2")
