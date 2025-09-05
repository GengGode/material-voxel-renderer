#pragma once
#if defined(_WIN32)
    #ifdef material_voxel_renderer_EXPORTS
        #define material_voxel_renderer_API __declspec(dllexport)
    #else
        #define material_voxel_renderer_API __declspec(dllimport)
    #endif
#else
    #ifdef material_voxel_renderer_EXPORTS
        #define material_voxel_renderer_API __attribute__((visibility("default")))
    #else
        #define material_voxel_renderer_API
    #endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    material_voxel_renderer_API const char* get_version();

#ifdef __cplusplus
}
#endif

#undef material_voxel_renderer_API
