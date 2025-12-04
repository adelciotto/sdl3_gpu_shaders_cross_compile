enum Resource_ID {
  RESOURCE_ID_SHADER_VERTEX_FULLSCREEN,
  RESOURCE_ID_SHADER_FRAGMENT_FBM_WARP,
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

static constexpr auto RESOURCES_INFO = []() {
  std::array<Resource_Info, RESOURCE_ID_COUNT> result = {};
  {
    auto info          = &result[RESOURCE_ID_SHADER_VERTEX_FULLSCREEN];
    info->kind         = RESOURCE_KIND_SHADER;
    info->file_name    = "fullscreen";
    info->shader.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  }
  {
    auto info                          = &result[RESOURCE_ID_SHADER_FRAGMENT_FBM_WARP];
    info->kind                         = RESOURCE_KIND_SHADER;
    info->file_name                    = "fbm_warp";
    info->shader.stage                 = SDL_GPU_SHADERSTAGE_FRAGMENT;
    info->shader.uniform_buffers_count = 1;
  }
  return result;
}();

static bool resource_load(
    Resources*           resources,
    Resource*            resource,
    SDL_GPUDevice*       device,
    const std::string&   base_path,
    const Resource_Info& resource_info) {
  switch (resource_info.kind) {
  case RESOURCE_KIND_SHADER: {
    std::vector<uint8_t> shader_code;
#ifdef BUILD_DEBUG
    resource->file_path = base_path + "src/" + resource_info.file_name + ".hlsl";

    std::string file_contents;
    if (!read_file_contents(resource->file_path.c_str(), &file_contents)) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to read file contents: %s",
          resource->file_path.c_str());
      return false;
    }

    SDL_ShaderCross_HLSL_Info hlsl_info = {};
    hlsl_info.source                    = file_contents.c_str();
    hlsl_info.entrypoint                = "main";
    hlsl_info.shader_stage              = resource_info.shader.stage == SDL_GPU_SHADERSTAGE_VERTEX
                                              ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX
                                              : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
    size_t   data_size;
    uint8_t* data;
    switch (resources->shader_format) {
    case SDL_GPU_SHADERFORMAT_DXIL: {
      data = static_cast<uint8_t*>(SDL_ShaderCross_CompileDXILFromHLSL(&hlsl_info, &data_size));
      if (data == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to compile DXIL from hlsl:\n%s",
            SDL_GetError());
        return false;
      }
    } break;
    case SDL_GPU_SHADERFORMAT_SPIRV: {
      data = static_cast<uint8_t*>(SDL_ShaderCross_CompileSPIRVFromHLSL(&hlsl_info, &data_size));
      if (data == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to compile SPIRV from hlsl:\n%s",
            SDL_GetError());
        return false;
      }
    } break;
    default:
      break;
    }
    SDL_assert(data != nullptr);
    shader_code.assign(data, data + data_size);
#else
    resource->file_path =
        base_path + "res/" + resource_info.file_name + "." + resources->shader_file_ext;

    if (!read_file_contents(resource->file_path.c_str(), &shader_code)) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to read file contents: %s",
          resource->file_path.c_str());
      return false;
    }
#endif

    SDL_GPUShaderCreateInfo info = {};
    info.code                    = shader_code.data();
    info.code_size               = shader_code.size();
    info.entrypoint              = "main";
    info.format                  = resources->shader_format;
    info.num_samplers            = resource_info.shader.samplers_count;
    info.num_storage_textures    = resource_info.shader.storage_textures_count;
    info.num_storage_buffers     = resource_info.shader.storage_buffers_count;
    info.num_uniform_buffers     = resource_info.shader.uniform_buffers_count;
    info.stage                   = resource_info.shader.stage;
    resource->shader.handle      = SDL_CreateGPUShader(device, &info);
    if (resource->shader.handle == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create shader: %s", SDL_GetError());
      return false;
    }
  } break;
  default:
    break;
  }

  return true;
}

static void resource_destroy(Resources* resources, Resource* resource, SDL_GPUDevice* device) {
  switch (resource->kind) {
  case RESOURCE_KIND_SHADER: {
    SDL_ReleaseGPUShader(device, resource->shader.handle);
  } break;
  default:
    break;
  }
}

static bool
resources_load(Resources* resources, SDL_GPUDevice* device, const std::string& base_path) {
  SDL_assert(resources != nullptr);
  SDL_assert(device != nullptr);

  auto device_shader_formats = SDL_GetGPUShaderFormats(device);
  if ((device_shader_formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
    resources->shader_format   = SDL_GPU_SHADERFORMAT_DXIL;
    resources->shader_file_ext = "dxil";
  } else if ((device_shader_formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
    resources->shader_format   = SDL_GPU_SHADERFORMAT_MSL;
    resources->shader_file_ext = "msl";
  } else if ((device_shader_formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
    resources->shader_format   = SDL_GPU_SHADERFORMAT_SPIRV;
    resources->shader_file_ext = "spv";
  } else {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "SDL GPU device does not support provided shader formats");
    return false;
  }

  for (int i = 0; i < RESOURCE_ID_COUNT; i++) {
    auto        resource      = &resources->items[i];
    const auto& resource_info = RESOURCES_INFO[i];
    if (!resource_load(resources, resource, device, base_path, resource_info)) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to load resource: kind=%d, file_name:%s",
          resource_info.kind,
          resource_info.file_name);
      return false;
    }

    SDL_PathInfo path_info;
    if (!SDL_GetPathInfo(resource->file_path.c_str(), &path_info)) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to get resource file path info: %s",
          SDL_GetError());
      return false;
    }
    resource->last_modify_time = path_info.modify_time;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loaded resource %s", resource->file_path.c_str());
  }

  SDL_GetCurrentTime(&resources->last_time);

  return true;
}

static void resources_live_reload(
    Resources*                                  resources,
    SDL_GPUDevice*                              device,
    const std::string&                          base_path,
    std::array<Resource_ID, RESOURCE_ID_COUNT>* out_modified_resource_ids,
    int*                                        out_modified_resource_ids_count) {
  SDL_assert(resources != nullptr);
  SDL_assert(device != nullptr);
  SDL_assert(out_modified_resource_ids != nullptr);
  SDL_assert(out_modified_resource_ids_count != nullptr);

  *out_modified_resource_ids_count = 0;

  SDL_Time current_time;
  SDL_GetCurrentTime(&current_time);
  auto delta_time = current_time - resources->last_time;
  if (SDL_NS_TO_SECONDS(static_cast<float>(delta_time)) < 0.5f) { return; }

  for (int i = 0; i < RESOURCE_ID_COUNT; i++) {
    const auto& resource_info = RESOURCES_INFO[i];

    SDL_PathInfo path_info;
    if (!SDL_GetPathInfo(resources->items[i].file_path.c_str(), &path_info)) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to get resource file path info: %s",
          SDL_GetError());
      continue;
    }

    if (resources->items[i].last_modify_time == path_info.modify_time) { continue; }
    resources->items[i].last_modify_time = path_info.modify_time;

    Resource resource = resources->items[i];
    if (!resource_load(resources, &resource, device, base_path, resource_info)) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to live reload resource: kind=%d, file_name:%s",
          resource_info.kind,
          resource_info.file_name);
      continue;
    }

    resource_destroy(resources, &resources->items[i], device);
    resources->items[i] = resource;

    (*out_modified_resource_ids)[*out_modified_resource_ids_count] = static_cast<Resource_ID>(i);
    *out_modified_resource_ids_count += 1;

    SDL_LogInfo(
        SDL_LOG_CATEGORY_APPLICATION,
        "Live reloaded resource %s",
        resources->items[i].file_path.c_str());
  }

  resources->last_time = current_time;
}

static void resources_destroy(Resources* resources, SDL_GPUDevice* device) {
  SDL_assert(resources != nullptr);
  SDL_assert(device != nullptr);

  for (int i = 0; i < RESOURCE_ID_COUNT; i++) {
    resource_destroy(resources, &resources->items[i], device);
  }
}

static const Resource& resources_get(const Resources& resources, Resource_ID id) {
  const auto& resource = resources.items[id];
  switch (resource.kind) {
  case RESOURCE_KIND_SHADER:
    SDL_assert(resource.shader.handle != nullptr);
    break;
  default:
    break;
  }

  return resource;
}
