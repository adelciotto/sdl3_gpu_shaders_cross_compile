#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <string>

enum Resource_ID {
  RESOURCE_ID_SHADER_VERTEX_FULLSCREEN,
  RESOURCE_ID_SHADER_FRAGMENT_FBM_WARP,
  RESOURCE_ID_SHADER_FRAGMENT_PLASMA_BEAT,
  RESOURCE_ID_COUNT,
};

enum Resource_Kind {
  RESOURCE_KIND_SHADER,
};

struct Resource_Info {
  Resource_Kind kind;
  const char*   file_name;
  struct {
    SDL_GPUShaderStage stage;
    int                samplers_count;
    int                storage_textures_count;
    int                storage_buffers_count;
    int                uniform_buffers_count;
  } shader;
};

struct Resource {
  Resource_Kind kind;
  std::string   file_path;
  SDL_Time      last_modify_time;
  struct {
    SDL_GPUShader* handle;
  } shader;
};

struct Resources {
  std::array<Resource, RESOURCE_ID_COUNT> items;
  SDL_GPUShaderFormat                     shader_format;
  const char*                             shader_file_ext;
  SDL_Time                                last_time;
};

extern const std::array<Resource_Info, RESOURCE_ID_COUNT> RESOURCES_INFO;

bool resources_load(Resources* resources, SDL_GPUDevice* device, SDL_Storage* storage);
void resources_live_reload(
    Resources*                                  resources,
    SDL_GPUDevice*                              device,
    SDL_Storage*                                storage,
    std::array<Resource_ID, RESOURCE_ID_COUNT>* out_modified_resource_ids,
    int*                                        out_modified_resource_ids_count);
void            resources_destroy(Resources* resources, SDL_GPUDevice* device);
const Resource& resources_get(const Resources& resources, Resource_ID id);
