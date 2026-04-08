#include <node_api.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <stdlib.h>

typedef struct { void *addr; size_t len; } mmap_ref;

static void mmap_release(napi_env env, void *data, void *hint) {
  mmap_ref *ref = (mmap_ref *)hint;
  if (ref->addr) {
    UnmapViewOfFile(ref->addr);
    napi_adjust_external_memory(env, -(int64_t)ref->len, NULL);
  }
  free(ref);
}

static napi_value do_mmap(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  napi_get_cb_info(env, info, &argc, args, NULL, NULL);

  int32_t fd;
  napi_get_value_int32(env, args[0], &fd);
  int64_t size;
  napi_get_value_int64(env, args[1], &size);
  int32_t writable = 0;
  if (argc > 2) napi_get_value_int32(env, args[2], &writable);

  HANDLE fh = (HANDLE)_get_osfhandle(fd);
  if (fh == INVALID_HANDLE_VALUE) {
    napi_throw_error(env, NULL, "Invalid file handle");
    return NULL;
  }

  DWORD protect = writable ? PAGE_READWRITE : PAGE_READONLY;
  DWORD access = writable ? FILE_MAP_WRITE : FILE_MAP_READ;
  DWORD sizeHi = (DWORD)((uint64_t)size >> 32);
  DWORD sizeLo = (DWORD)(size & 0xFFFFFFFF);

  HANDLE mapping = CreateFileMappingA(fh, NULL, protect, sizeHi, sizeLo, NULL);
  if (!mapping) {
    napi_throw_error(env, NULL, "CreateFileMapping failed");
    return NULL;
  }

  void *ptr = MapViewOfFile(mapping, access, 0, 0, (SIZE_T)size);
  CloseHandle(mapping);
  if (!ptr) {
    napi_throw_error(env, NULL, "MapViewOfFile failed");
    return NULL;
  }

  mmap_ref *ref = (mmap_ref *)malloc(sizeof(mmap_ref));
  ref->addr = ptr;
  ref->len = (size_t)size;

  napi_value buf;
  napi_create_external_buffer(env, (size_t)size, ptr, mmap_release, ref, &buf);
  napi_adjust_external_memory(env, (int64_t)size, NULL);
  return buf;
}

static napi_value do_munmap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  void *data; size_t len;
  napi_get_buffer_info(env, args[0], &data, &len);
  if (data) {
    UnmapViewOfFile(data);
    napi_adjust_external_memory(env, -(int64_t)len, NULL);
  }
  napi_value u; napi_get_undefined(env, &u); return u;
}

static napi_value do_msync(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  void *data; size_t len;
  napi_get_buffer_info(env, args[0], &data, &len);
  if (data && len > 0) FlushViewOfFile(data, len);
  napi_value u; napi_get_undefined(env, &u); return u;
}

#else
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

typedef struct { void *addr; size_t len; } mmap_ref;

static void mmap_release(napi_env env, void *data, void *hint) {
  mmap_ref *ref = (mmap_ref *)hint;
  if (ref->addr && ref->len > 0) {
    munmap(ref->addr, ref->len);
    napi_adjust_external_memory(env, -(int64_t)ref->len, NULL);
  }
  free(ref);
}

static napi_value do_mmap(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  napi_get_cb_info(env, info, &argc, args, NULL, NULL);

  int32_t fd;
  napi_get_value_int32(env, args[0], &fd);
  int64_t size;
  napi_get_value_int64(env, args[1], &size);
  int32_t writable = 0;
  if (argc > 2) napi_get_value_int32(env, args[2], &writable);

  int prot = writable ? (PROT_READ | PROT_WRITE) : PROT_READ;
  int flags = MAP_SHARED;
  void *ptr = mmap(NULL, (size_t)size, prot, flags, fd, 0);
  if (ptr == MAP_FAILED) {
    napi_throw_error(env, NULL, strerror(errno));
    return NULL;
  }
  madvise(ptr, (size_t)size, MADV_RANDOM);

#ifdef __APPLE__
  fcntl(fd, F_RDAHEAD, 1);
#endif

  mmap_ref *ref = (mmap_ref *)malloc(sizeof(mmap_ref));
  ref->addr = ptr;
  ref->len = (size_t)size;

  napi_value buf;
  napi_create_external_buffer(env, (size_t)size, ptr, mmap_release, ref, &buf);
  napi_adjust_external_memory(env, (int64_t)size, NULL);
  return buf;
}

static napi_value do_munmap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  void *data; size_t len;
  napi_get_buffer_info(env, args[0], &data, &len);
  if (data && len > 0) {
    munmap(data, len);
    napi_adjust_external_memory(env, -(int64_t)len, NULL);
  }
  napi_value u; napi_get_undefined(env, &u); return u;
}

static napi_value do_msync(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  void *data; size_t len;
  napi_get_buffer_info(env, args[0], &data, &len);
  if (data && len > 0) msync(data, len, MS_ASYNC);
  napi_value u; napi_get_undefined(env, &u); return u;
}

#endif

static napi_value init(napi_env env, napi_value exports) {
  napi_value fn;
  napi_create_function(env, "mmap", 4, do_mmap, NULL, &fn);
  napi_set_named_property(env, exports, "mmap", fn);
  napi_create_function(env, "munmap", 6, do_munmap, NULL, &fn);
  napi_set_named_property(env, exports, "munmap", fn);
  napi_create_function(env, "msync", 5, do_msync, NULL, &fn);
  napi_set_named_property(env, exports, "msync", fn);
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
