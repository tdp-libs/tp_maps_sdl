#include "tp_maps_sdl/Map.h"
#include "tp_maps_sdl/Vulkan.h"

#include "tp_maps/MouseEvent.h"
#include "tp_maps/KeyEvent.h"

#include "tp_utils/TimeUtils.h"
#include "tp_utils/DebugUtils.h"

namespace tp_maps_sdl
{
//##################################################################################################
struct Map::Private
{
  Q* q;
  SDL_Window* window{nullptr};

  glm::ivec2 mousePos{0,0};

  int64_t animationTimeMS{0};

  bool paint{true};
  bool quitting{false};


  //-- OpenGL --------------------------------------------------------------------------------------
  SDL_GLContext context{nullptr};


  //-- Vulkan --------------------------------------------------------------------------------------
  std::unique_ptr<Vulkan> vulkan;


  //################################################################################################
  Private(Q* q_):
    q(q_)
  {

  }

  //################################################################################################
  SDL_Rect getActiveDisplayBounds()
  {
    int x=0;
    int y=0;
    SDL_GetGlobalMouseState(&x, &y);

    SDL_Rect displayBounds;
    displayBounds.x = SDL_WINDOWPOS_UNDEFINED;
    displayBounds.y = SDL_WINDOWPOS_UNDEFINED;
    displayBounds.w = 512;
    displayBounds.h = 512;

    int displayIndex = 0;

    int nMax = SDL_GetNumVideoDisplays();
    for(int n=0; n<nMax; n++)
    {
      SDL_Rect b;
      SDL_GetDisplayUsableBounds(n, &b);

      if(x>=b.x && x<(b.x+b.w) && y>=b.y && y<(b.y+b.h))
      {
        displayIndex = n;
        break;
      }
    }

    if(nMax>0)
      SDL_GetDisplayUsableBounds(displayIndex, &displayBounds);

    return displayBounds;
  }

  //################################################################################################
  void initGL(bool fullScreen, const std::string& title)
  {
    SDL_Rect s = getActiveDisplayBounds();

    auto tryMakeWindow = [&](const auto& setWindowOps)
    {
      setWindowOps();

#if defined(TP_ANDROID) || defined(TP_IOS)
      d->window = SDL_CreateWindow(title.c_str(),
                                   s.x,
                                   s.y,
                                   s.w,
                                   s.h,
                                   SDL_WINDOW_OPENGL);
#else
      if(fullScreen)
      {
        window = SDL_CreateWindow(title.c_str(),
                                  s.x,
                                  s.y,
                                  s.w,
                                  s.h,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_SetRelativeMouseMode(SDL_TRUE);
      }
      else
      {
        window = SDL_CreateWindow(title.c_str(),
                                  s.x,
                                  s.y,
                                  s.w,
                                  s.h,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
      }
#endif

      if(!window)
        return;

      context = SDL_GL_CreateContext(window);

      if(!context)
      {
        SDL_DestroyWindow(window);
        window = nullptr;
      }
    };

#if defined(TP_GLES2)
    tryMakeWindow([&]{d->opsForGLES2();});
#endif

#if defined(TP_OSX)
    if(!d->context)tryMakeWindow([&]{d->opsForGL4_1();});
#endif

#if !defined(TP_ANDROID) && !defined(TP_IOS)
    if(!context)tryMakeWindow([&]{opsForGL3_3();});
    if(!context)tryMakeWindow([&]{opsForGL2_1();});
#endif
    if(!context)tryMakeWindow([&]{opsForGLES2();});

    if(!window)
    {
      tpWarning() << "Failed to create SDL window: " << SDL_GetError();
      return;
    }

    if(!context)
    {
      tpWarning() << "Failed to create OpenGL context: " << SDL_GetError();
      return;
    }

    SDL_GL_SetSwapInterval(-1);

    q->initializeGL();
  }

  //################################################################################################
  void initVK(bool fullScreen, const std::string& title)
  {
    SDL_Rect s = getActiveDisplayBounds();

    auto tryMakeWindow = [&](const auto& setWindowOps)
    {
      setWindowOps();

#if defined(TP_ANDROID) || defined(TP_IOS)
      d->window = SDL_CreateWindow(title.c_str(),
                                   s.x,
                                   s.y,
                                   s.w,
                                   s.h,
                                   SDL_WINDOW_VULKAN);
#else
      if(fullScreen)
      {
        window = SDL_CreateWindow(title.c_str(),
                                  s.x,
                                  s.y,
                                  s.w,
                                  s.h,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_SetRelativeMouseMode(SDL_TRUE);
      }
      else
      {
        window = SDL_CreateWindow(title.c_str(),
                                  s.x,
                                  s.y,
                                  s.w,
                                  s.h,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
      }
#endif

      if(!window)
        return;

      vulkan = std::make_unique<Vulkan>(window, title);
    };

    tryMakeWindow([&]{opsForVulkan();});

    q->initializeGL();
  }

  //################################################################################################
  void update()
  {
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
      switch(event.type)
      {
        case SDL_QUIT: //---------------------------------------------------------------------------
        {
          quitting = true;
          break;
        }

        case SDL_MOUSEBUTTONDOWN: //----------------------------------------------------------------
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

        case SDL_MOUSEBUTTONUP: //------------------------------------------------------------------
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

        case SDL_MOUSEMOTION: //--------------------------------------------------------------------
        {
          tp_maps::MouseEvent e(tp_maps::MouseEventType::Move);
          mousePos = {event.motion.x, event.motion.y};
          e.pos = mousePos;
          e.posDelta = {event.motion.xrel, event.motion.yrel};
          q->mouseEvent(e);
          break;
        }

        case SDL_MOUSEWHEEL: //---------------------------------------------------------------------
        {
          tp_maps::MouseEvent e(tp_maps::MouseEventType::Wheel);
          e.pos = mousePos;
          e.delta = event.wheel.y;
          q->mouseEvent(e);
          break;
        }

        case SDL_WINDOWEVENT: //--------------------------------------------------------------------
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

        case SDL_KEYDOWN: //------------------------------------------------------------------------
        {
          tp_maps::KeyEvent e(tp_maps::KeyEventType::Press);
          e.scancode = event.key.keysym.scancode;
          q->keyEvent(e);
          break;
        }

        case SDL_KEYUP: //--------------------------------------------------------------------------
        {
          if(event.key.keysym.scancode == 41)
            quitting = true;

          tp_maps::KeyEvent e(tp_maps::KeyEventType::Release);
          e.scancode = event.key.keysym.scancode;
          q->keyEvent(e);
          break;
        }

        case SDL_TEXTINPUT: //----------------------------------------------------------------------
        {
          tp_maps::TextInputEvent e;
          e.text = event.text.text;
          q->textInputEvent(e);
          break;
        }

        case SDL_TEXTEDITING: //--------------------------------------------------------------------
        {
          tp_maps::TextEditingEvent e;
          e.text           = event.edit.text;
          e.cursor         = event.edit.start;
          e.selectionLength = event.edit.length;
          q->textEditingEvent(e);
          break;
        }

        default: //---------------------------------------------------------------------------------
        {
          break;
        }
      }
    }

    if(const auto t = tp_utils::currentTimeMS(); animationTimeMS<t)
    {
      animationTimeMS = t+8;
      q->makeCurrent();
      q->animate(double(t));
    }

    if(paint)
    {
      paint = false;
      q->makeCurrent();
      q->paintGL();
      SDL_GL_SwapWindow(window);
    }
  }

  //################################################################################################
  void opsForGL4_1()
  {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    q->setShaderProfile(tp_maps::ShaderProfile::GLSL_410);
  }

  //################################################################################################
  void opsForGL3_3()
  {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    q->setShaderProfile(tp_maps::ShaderProfile::GLSL_330);
  }

  //################################################################################################
  void opsForGL2_1()
  {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    q->setShaderProfile(tp_maps::ShaderProfile::GLSL_120);
  }

  //################################################################################################
  void opsForGLES2()
  {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    q->setShaderProfile(tp_maps::ShaderProfile::GLSL_100_ES);
  }

  //################################################################################################
  void opsForVulkan()
  {

#warning set this correctly
    q->setShaderProfile(tp_maps::ShaderProfile::GLSL_100_ES);
  }
};

//##################################################################################################
Map::Map(bool enableDepthBuffer, bool fullScreen, const std::string& title):
  tp_maps::Map(enableDepthBuffer),
  d(new Private(this))
{
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) != 0)
  {
    tpWarning() << "Failed to initialize SDL: " << SDL_GetError();
    return;
  }

  d->initGL(fullScreen, title);

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

  SDL_GL_DeleteContext(d->context);
  SDL_DestroyWindow(d->window);
  SDL_Quit();

  delete d;
}

//##################################################################################################
void Map::exec()
{
  while(!d->quitting)
  {
    processEvents();
    SDL_Delay(16);
  }
}

//##################################################################################################
void Map::processEvents()
{
  d->update();
}

//##################################################################################################
void Map::makeCurrent()
{
  SDL_GL_MakeCurrent(d->window, d->context);
}

//##################################################################################################
void Map::update(tp_maps::RenderFromStage renderFromStage)
{
  tp_maps::Map::update(renderFromStage);
  d->paint = true;
}

//##################################################################################################
void Map::callAsync(const std::function<void()>& callback)
{
#warning implement
}

//##################################################################################################
void Map::setRelativeMouseMode(bool enabled)
{
  SDL_SetRelativeMouseMode(enabled?SDL_TRUE:SDL_FALSE);
}

//##################################################################################################
bool Map::relativeMouseMode() const
{
  return SDL_GetRelativeMouseMode() == SDL_TRUE;
}

//##################################################################################################
void Map::startTextInput()
{
  SDL_StartTextInput();
}

//##################################################################################################
void Map::stopTextInput()
{
  SDL_StopTextInput();
}


}
