// -- External Header Includes ------------------------------------------------
#include <HandmadeMath.h>
#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#ifdef BUILD_DEBUG
#include <SDL3_shadercross/SDL_shadercross.h>
#endif

// -- Std Header Includes -----------------------------------------------------
#include <array>
#include <string>
#include <vector>

// -- Local Source Includes ---------------------------------------------------
#include "common.cpp"
#include "imgui_font.cpp"
#include "resources.cpp"

enum Shader_Kind {
  SHADER_KIND_FBM_WARP,
  SHADER_KIND_PLASMA_BEAT,
  SHADER_KIND_COUNT,
};

struct Shader_FBM_Warp_Uniforms {
  float    time;
  HMM_Vec2 resolution;
};

struct App_State {
  SDL_Storage*         title_storage;
  SDL_GPUDevice*       device;
  SDL_Window*          window;
  SDL_GPUTextureFormat swapchain_texture_format;
  float                content_scale;
  HMM_Vec2             window_size_pixels;
  bool                 window_minimized;
  uint64_t             count_per_second;
  uint64_t             last_counter;
  uint64_t             max_counter_delta;
  double               elapsed_time;
  bool                 vsync      = true;
  bool                 fullscreen = false;

  ImFont* imgui_font;

  Shader_Kind                                             shader_kind = SHADER_KIND_FBM_WARP;
  Resources                                               resources;
  std::array<SDL_GPUGraphicsPipeline*, SHADER_KIND_COUNT> pipelines;
  SDL_GPUTexture*                                         render_target;
  int                                                     render_scale_index;
  HMM_Vec2                                                render_size;
};

static constexpr std::array<Resource_ID, SHADER_KIND_COUNT> SHADER_KIND_RESOURCE_IDS = {
    {
        RESOURCE_ID_SHADER_FRAGMENT_FBM_WARP,
        RESOURCE_ID_SHADER_FRAGMENT_PLASMA_BEAT,
    },
};

static constexpr std::array RENDER_TARGET_SCALE_VALUES = {
    1.0f,
    0.9f,
    0.8f,
    0.75f,
    0.5f,
};

static bool init_render_texture(App_State* as) {
  as->render_size = as->window_size_pixels * RENDER_TARGET_SCALE_VALUES[as->render_scale_index];

  SDL_GPUTexture* render_target;
  {
    SDL_GPUTextureCreateInfo info = {};
    info.type                     = SDL_GPU_TEXTURETYPE_2D;
    info.width                    = static_cast<int>(as->render_size.X);
    info.height                   = static_cast<int>(as->render_size.Y);
    info.layer_count_or_depth     = 1;
    info.num_levels               = 1;
    info.format                   = as->swapchain_texture_format;
    info.usage    = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    render_target = SDL_CreateGPUTexture(as->device, &info);
    if (render_target == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s", SDL_GetError());
      return false;
    }
  };

  SDL_ReleaseGPUTexture(as->device, as->render_target);
  as->render_target = render_target;

  return true;
}

static bool init_pipeline(App_State* as, Shader_Kind shader_kind) {
  SDL_GPUColorTargetDescription desc = {};
  desc.format                        = as->swapchain_texture_format;

  SDL_GPUGraphicsPipelineCreateInfo info     = {};
  info.target_info.num_color_targets         = 1;
  info.target_info.color_target_descriptions = &desc;
  info.primitive_type                        = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  info.vertex_shader =
      resources_get(as->resources, RESOURCE_ID_SHADER_VERTEX_FULLSCREEN).shader.handle;
  info.fragment_shader =
      resources_get(as->resources, SHADER_KIND_RESOURCE_IDS[shader_kind]).shader.handle;
  auto pipeline = SDL_CreateGPUGraphicsPipeline(as->device, &info);
  if (pipeline == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create pipeline: %s", SDL_GetError());
    return false;
  }

  SDL_ReleaseGPUGraphicsPipeline(as->device, as->pipelines[shader_kind]);
  as->pipelines[shader_kind] = pipeline;

  return true;
}

static bool on_window_pixel_size_changed(App_State* as, int width, int height) {
  if (as->window_size_pixels.X == width && as->window_size_pixels.Y == height) { return true; }
  as->window_size_pixels = HMM_V2(width, height);

  if (!init_render_texture(as)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create render target");
    return false;
  }

  return true;
}

static void on_display_content_scale_changed(App_State* as, float content_scale) {
  as->content_scale = content_scale;

  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(as->content_scale);
  style.FontScaleDpi = as->content_scale;
}

static void on_vsync_changed(App_State* as, bool vsync) {
  as->vsync = vsync;

  SDL_GPUPresentMode present_mode =
      vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
  SDL_SetGPUSwapchainParameters(
      as->device,
      as->window,
      SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
      present_mode);
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  auto as = new (std::nothrow) App_State {};
  if (as == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate App_State");
    return SDL_APP_FAILURE;
  }
  *appstate = as;

#ifdef BUILD_DEBUG
  std::string base_path = RESOURCES_PATH;
#else
  auto base_path_ptr = SDL_GetBasePath();
  if (base_path_ptr == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get base path: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  std::string base_path = base_path_ptr;
#endif
  as->title_storage = SDL_OpenTitleStorage(base_path.c_str(), 0);
  if (as->title_storage == nullptr) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to get open title stotage: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }
  while (!SDL_StorageReady(as->title_storage)) { SDL_Delay(1); }

  SDL_GPUShaderFormat format_flags = 0;
#ifdef SDL_PLATFORM_WINDOWS
  format_flags |= SDL_GPU_SHADERFORMAT_DXIL;
#elif SDL_PLATFORM_LINUX
  format_flags |= SDL_GPU_SHADERFORMAT_SPIRV;
#else
#error "Platform not supported"
#endif
  bool debug = false;
#ifdef BUILD_DEBUG
  debug = true;
#endif
  as->device = SDL_CreateGPUDevice(format_flags, debug, nullptr);
  if (as->device == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create gpu device: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  int   window_width       = 800;
  int   window_height      = 600;
  float content_scale      = 1.0f;
  auto  primary_display_id = SDL_GetPrimaryDisplay();
  if (primary_display_id != 0) {
    auto display_mode = SDL_GetDesktopDisplayMode(primary_display_id);
    if (display_mode != nullptr) {
      window_width  = display_mode->w / 2;
      window_height = display_mode->h / 2;
    } else {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to get primary display mode: %s",
          SDL_GetError());
    }
    content_scale = SDL_GetDisplayContentScale(primary_display_id);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get primary display: %s", SDL_GetError());
  }

  as->window = SDL_CreateWindow(
      "SDL3 GPU SHADERS CROSS COMPILE",
      window_width,
      window_height,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (as->window == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(as->device, as->window)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to claim window for gpu device: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }

  as->swapchain_texture_format = SDL_GetGPUSwapchainTextureFormat(as->device, as->window);

  {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    auto& style         = ImGui::GetStyle();
    style.ItemSpacing.y = 8.0f;

    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    as->imgui_font                = io.Fonts->AddFontFromMemoryCompressedTTF(
        const_cast<unsigned char*>(IMGUI_FONT_DATA),
        IMGUI_FONT_DATA_SIZE,
        18.0f,
        &font_cfg);

    on_display_content_scale_changed(as, content_scale);
  }

  {
    ImGui_ImplSDL3_InitForSDLGPU(as->window);

    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device                     = as->device;
    init_info.ColorTargetFormat          = SDL_GetGPUSwapchainTextureFormat(as->device, as->window);
    ImGui_ImplSDLGPU3_Init(&init_info);
  }

  if (!resources_load(&as->resources, as->device, as->title_storage)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load resources");
    return SDL_APP_FAILURE;
  }

  for (int i = 0; i < SHADER_KIND_COUNT; i++) {
    if (!init_pipeline(as, static_cast<Shader_Kind>(i))) { return SDL_APP_FAILURE; }
  }

  on_vsync_changed(as, as->vsync);
  {
    int w, h;
    SDL_GetWindowSizeInPixels(as->window, &w, &h);
    if (!on_window_pixel_size_changed(as, w, h)) { return SDL_APP_FAILURE; }
  }

  as->count_per_second  = SDL_GetPerformanceFrequency();
  as->last_counter      = SDL_GetPerformanceCounter();
  as->max_counter_delta = as->count_per_second / 60 * 8;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto as = static_cast<App_State*>(appstate);

  auto& io                  = ImGui::GetIO();
  bool  process_imgui_event = true;

  switch (event->type) {
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    on_window_pixel_size_changed(as, event->window.data1, event->window.data2);
    break;
  case SDL_EVENT_WINDOW_MINIMIZED:
    as->window_minimized = true;
    break;
  case SDL_EVENT_WINDOW_RESTORED:
    as->window_minimized = false;
    break;
  case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
    on_display_content_scale_changed(as, SDL_GetDisplayContentScale(event->display.displayID));
    break;
  default:
    break;
  }

  if (process_imgui_event) { ImGui_ImplSDL3_ProcessEvent(event); }

  return SDL_APP_CONTINUE;
}

static void draw_imgui(App_State* as) {
  if (ImGui::Begin(
          "SDL3 GPU Shaders Cross Compile Demo",
          nullptr,
          ImGuiWindowFlags_HorizontalScrollbar)) {
    auto& io = ImGui::GetIO();
    ImGui::Text(
        "Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / io.Framerate,
        io.Framerate);

    bool vsync = as->vsync;
    if (ImGui::Checkbox("VSync", &vsync)) { on_vsync_changed(as, vsync); }

    if (ImGui::Button("Toggle Fullscreen")) {
      as->fullscreen = !as->fullscreen;
      SDL_SetWindowFullscreen(as->window, as->fullscreen);
    }

    ImGui::Separator();

    static constexpr const char* shader_kind_strings[SHADER_KIND_COUNT] = {
        "FBM Warp",
        "Plasma Beat",
    };
    if (ImGui::BeginCombo("Shader Selection", shader_kind_strings[as->shader_kind])) {
      for (int i = 0; i < SHADER_KIND_COUNT; i++) {
        bool is_selected = as->shader_kind == i;
        if (ImGui::Selectable(shader_kind_strings[i], is_selected)) {
          as->shader_kind = static_cast<Shader_Kind>(i);
        }
        if (is_selected) { ImGui::SetItemDefaultFocus(); }
      }
      ImGui::EndCombo();
    }

    static constexpr std::array<const char*, RENDER_TARGET_SCALE_VALUES.size()>
        render_scale_strings = {
            "100%",
            "90%",
            "80%",
            "75%",
            "50%",
        };
    if (ImGui::BeginCombo("Render Scale", render_scale_strings[as->render_scale_index])) {
      for (int i = 0; i < RENDER_TARGET_SCALE_VALUES.size(); i++) {
        bool is_selected = as->render_scale_index == i;
        if (ImGui::Selectable(render_scale_strings[i], is_selected)) {
          if (as->render_scale_index != i) {
            as->render_scale_index = i;
            if (!init_render_texture(as)) {
              SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create render target");
            }
          }
        }
        if (is_selected) { ImGui::SetItemDefaultFocus(); }
      }
      ImGui::EndCombo();
    }
  }
  ImGui::End();
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto as = static_cast<App_State*>(appstate);

#ifdef BUILD_DEBUG
  std::array<Resource_ID, RESOURCE_ID_COUNT> modified_resource_ids;
  int                                        modified_resource_ids_count;
  resources_live_reload(
      &as->resources,
      as->device,
      as->title_storage,
      &modified_resource_ids,
      &modified_resource_ids_count);

  for (auto id : modified_resource_ids) {
    switch (id) {
    case RESOURCE_ID_SHADER_VERTEX_FULLSCREEN: {
      for (int i = 0; i < SHADER_KIND_COUNT; i++) {
        init_pipeline(as, static_cast<Shader_Kind>(i));
      }
    } break;
    case RESOURCE_ID_SHADER_FRAGMENT_FBM_WARP: {
      init_pipeline(as, SHADER_KIND_FBM_WARP);
    } break;
    case RESOURCE_ID_SHADER_FRAGMENT_PLASMA_BEAT: {
      init_pipeline(as, SHADER_KIND_PLASMA_BEAT);
    } break;
    default:
      break;
    }
  }
#endif

  auto counter       = SDL_GetPerformanceCounter();
  auto counter_delta = counter - as->last_counter;
  as->last_counter   = counter;
  if (counter_delta > as->max_counter_delta) { counter_delta = as->count_per_second / 60; }

  auto delta_time = static_cast<double>(counter_delta) / static_cast<double>(as->count_per_second);
  as->elapsed_time += delta_time;

  static constexpr double TIME_RESET_PERIOD = 3600.0;
  if (as->elapsed_time >= TIME_RESET_PERIOD) { as->elapsed_time = 0.0; }

  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  draw_imgui(as);
  ImGui::Render();

  SDL_GPUCommandBuffer* cmd_buf = SDL_AcquireGPUCommandBuffer(as->device);
  if (cmd_buf == nullptr) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to acquire command buffer: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUTexture* swapchain_texture;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
          cmd_buf,
          as->window,
          &swapchain_texture,
          nullptr,
          nullptr)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to acquire swapchain texture: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (swapchain_texture != nullptr && !as->window_minimized) {
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd_buf);

    {
      SDL_GPUColorTargetInfo target_info = {};
      target_info.texture                = as->render_target;
      target_info.load_op                = SDL_GPU_LOADOP_CLEAR;
      target_info.store_op               = SDL_GPU_STOREOP_STORE;
      SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmd_buf, &target_info, 1, nullptr);
      defer(SDL_EndGPURenderPass(render_pass));

      SDL_BindGPUGraphicsPipeline(render_pass, as->pipelines[as->shader_kind]);

      switch (as->shader_kind) {
      case SHADER_KIND_FBM_WARP:
      case SHADER_KIND_PLASMA_BEAT: {
        Shader_FBM_Warp_Uniforms uniforms = {};
        uniforms.time                     = static_cast<float>(as->elapsed_time);
        uniforms.resolution               = as->render_size;
        SDL_PushGPUFragmentUniformData(cmd_buf, 0, &uniforms, sizeof(uniforms));
      } break;
      default:
        break;
      }

      SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
    }

    {
      SDL_GPUBlitInfo info     = {};
      info.source.texture      = as->render_target;
      info.source.w            = static_cast<int>(as->render_size.X);
      info.source.h            = static_cast<int>(as->render_size.Y);
      info.destination.texture = swapchain_texture;
      info.destination.w       = static_cast<int>(as->window_size_pixels.X);
      info.destination.h       = static_cast<int>(as->window_size_pixels.Y);
      info.load_op             = SDL_GPU_LOADOP_DONT_CARE;
      info.filter              = SDL_GPU_FILTER_LINEAR;
      info.flip_mode           = SDL_FLIP_VERTICAL;
      SDL_BlitGPUTexture(cmd_buf, &info);
    }

    {
      SDL_GPUColorTargetInfo target_info = {};
      target_info.texture                = swapchain_texture;
      target_info.load_op                = SDL_GPU_LOADOP_DONT_CARE;
      target_info.store_op               = SDL_GPU_STOREOP_STORE;
      SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmd_buf, &target_info, 1, nullptr);
      defer(SDL_EndGPURenderPass(render_pass));

      ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd_buf, render_pass);
    }
  }

  SDL_SubmitGPUCommandBuffer(cmd_buf);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  auto as = static_cast<App_State*>(appstate);

  SDL_WaitForGPUIdle(as->device);

  resources_destroy(&as->resources, as->device);

  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();

  SDL_ReleaseWindowFromGPUDevice(as->device, as->window);
  SDL_DestroyWindow(as->window);
  SDL_DestroyGPUDevice(as->device);

  SDL_CloseStorage(as->title_storage);

  delete as;

  SDL_Quit();
}
