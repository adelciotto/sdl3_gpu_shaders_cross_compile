#include "util.h"

#include <string>
#include <vector>

// -- Storage -----------------------------------------------------------------

template<typename Container>
bool read_storage_file(SDL_Storage* storage, const char* file_path, Container* out_container) {
  uint64_t file_size = 0;
  if (!SDL_GetStorageFileSize(storage, file_path, &file_size)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to get storage file size: %s",
        SDL_GetError());
    return false;
  }
  if (file_size == 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get storage file size: file size is 0");
    return false;
  }

  out_container->resize(file_size);
  if (!SDL_ReadStorageFile(storage, file_path, out_container->data(), file_size)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read storage file: %s", SDL_GetError());
    return false;
  }

  return true;
}

template bool read_storage_file<std::string>(SDL_Storage*, const char*, std::string*);
template bool
read_storage_file<std::vector<uint8_t>>(SDL_Storage*, const char*, std::vector<uint8_t>*);
