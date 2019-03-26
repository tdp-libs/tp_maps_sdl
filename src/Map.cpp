#include "tp_maps_sdl/Map.h"

#include "tp_maps/MouseEvent.h"
#include "tp_maps/KeyEvent.h"

#include "tp_utils/TimeUtils.h"
#include "tp_utils/DebugUtils.h"

#ifdef EMSCRIPTEN
#include "SDL.h"
#include <SDL_image.h>
#include "emscripten.h"
#else
#include <SDL2/SDL.h>
#endif

namespace tp_maps_sdl
{
//##################################################################################################
struct Map::Private
{
  Map* q;
  SDL_Window *window{nullptr};
  SDL_GLContext context{nullptr};

  glm::ivec2 mousePos{0,0};

  int64_t animationTimeMS{0};

  bool paint{true};
  bool quitting{false};

  //################################################################################################
  Private(Map* q_):
    q(q_)
  {

  }

#ifdef EMSCRIPTEN
  //################################################################################################
  static void updateCallback(void* opaque)
  {
    static_cast<Private*>(opaque)->update();
  }
#endif

  //################################################################################################
  void update()
  {
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
      switch(event.type)
      {

      case SDL_QUIT: //-----------------------------------------------------------------------------
      {
        quitting = true;
        break;
      }

      case SDL_MOUSEBUTTONDOWN: //------------------------------------------------------------------
      {
        tp_maps::MouseEvent e(tp_maps::MouseEventType::Press);
        e.pos = {event.button.x, event.button.y};
        switch (event.button.button)
        {
        case SDL_BUTTON_LEFT:  e.button = tp_maps::Button::LeftButton;  break;
        case SDL_BUTTON_RIGHT: e.button = tp_maps::Button::RightButton; break;
        default:               e.button = tp_maps::Button::NoButton;    break;
        }
        q->mouseEvent(e);
        break;
      }

      case SDL_MOUSEBUTTONUP: //--------------------------------------------------------------------
      {
        tp_maps::MouseEvent e(tp_maps::MouseEventType::Release);
        e.pos = {event.button.x, event.button.y};
        switch (event.button.button)
        {
        case SDL_BUTTON_LEFT:  e.button = tp_maps::Button::LeftButton;  break;
        case SDL_BUTTON_RIGHT: e.button = tp_maps::Button::RightButton; break;
        default:               e.button = tp_maps::Button::NoButton;    break;
        }
        q->mouseEvent(e);
        break;
      }

      case SDL_MOUSEMOTION: //----------------------------------------------------------------------
      {
        tp_maps::MouseEvent e(tp_maps::MouseEventType::Move);
        mousePos = {event.motion.x, event.motion.y};        
        e.pos = mousePos;
        e.posDelta = {event.motion.xrel, event.motion.yrel};
        q->mouseEvent(e);
        break;
      }

      case SDL_MOUSEWHEEL: //-----------------------------------------------------------------------
      {
        tp_maps::MouseEvent e(tp_maps::MouseEventType::Wheel);
        e.pos = mousePos;
        e.delta = event.wheel.y;
        q->mouseEvent(e);
        break;
      }

      case SDL_WINDOWEVENT: //----------------------------------------------------------------------
      {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          q->resizeGL(event.window.data1, event.window.data2);
          paint = true;
        }
        else if (event.window.event == SDL_WINDOWEVENT_SHOWN || event.window.event == SDL_WINDOWEVENT_EXPOSED)
        {
          paint = true;
        }

        break;
      }

      case SDL_KEYDOWN: //--------------------------------------------------------------------------
      {
        tp_maps::KeyEvent e(tp_maps::KeyEventType::Press);
        e.scancode = event.key.keysym.scancode;
        q->keyEvent(e);
        break;
      }

      case SDL_KEYUP: //----------------------------------------------------------------------------
      {
        if(event.key.keysym.scancode == 41)
          quitting = true;

        tp_maps::KeyEvent e(tp_maps::KeyEventType::Release);
        e.scancode = event.key.keysym.scancode;
        q->keyEvent(e);
        break;
      }

      default: //-----------------------------------------------------------------------------------
      {
        break;
      }
      }
    }

    if(const auto t = tp_utils::currentTimeMS(); animationTimeMS<t)
    {
      animationTimeMS = t+8;
      q->makeCurrent();
      q->animate(t);
    }

    if(paint)
    {
      paint = false;
      q->makeCurrent();
      q->paintGL();
      SDL_GL_SwapWindow(window);
    }
  }
};

//##################################################################################################
Map::Map(bool enableDepthBuffer, bool fullScreen, const std::string& title):
  tp_maps::Map(enableDepthBuffer),
  d(new Private(this))
{
#ifdef EMSCRIPTEN
  SDL_Renderer *renderer = NULL;
  SDL_CreateWindowAndRenderer(512, 512, SDL_WINDOW_OPENGL, &d->window, &renderer);
#else
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) != 0)
  {
    tpWarning() << "Failed to initialize SDL: " << SDL_GetError();
    return;
  }

  // Use a core profile setup.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  //SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);

  //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

  if(fullScreen)
  {
    d->window = SDL_CreateWindow(title.c_str(),
                                 SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED,
                                 512,
                                 512,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_SetRelativeMouseMode(SDL_TRUE);
  }
  else
  {
    d->window = SDL_CreateWindow(title.c_str(),
                                 SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED,
                                 512,
                                 512,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  }

  if(!d->window)
  {
    tpWarning() << "Failed to create SDL window: " << SDL_GetError();
    return;
  }

  d->context = SDL_GL_CreateContext(d->window);
  SDL_GL_SetSwapInterval(1);
#endif

  initializeGL();

  {
    int w{0};
    int h{0};
    SDL_GetWindowSize(d->window, &w, &h);
    resizeGL(w, h);
  }
}

//##################################################################################################
Map::~Map()
{
  preDelete();

#ifdef EMSCRIPTEN

#else
  SDL_GL_DeleteContext(d->context);
  SDL_DestroyWindow(d->window);
  SDL_Quit();
#endif

  delete d;
}

//##################################################################################################
void Map::exec()
{
#ifdef EMSCRIPTEN
  emscripten_set_main_loop_arg(Private::updateCallback, d, 20, 1);
#else
  while(!d->quitting)
  {
    processEvents();
    SDL_Delay(2);
  }
#endif
}

//##################################################################################################
void Map::processEvents()
{
#ifdef EMSCRIPTEN
  tpWarning() << "Error Map::processEvents Does not work with Emscripten, use exec()";
#else
  d->update();
#endif
}

//##################################################################################################
void Map::makeCurrent()
{
  SDL_GL_MakeCurrent(d->window, d->context);
}

//##################################################################################################
void Map::update()
{
  d->paint = true;
}

}
