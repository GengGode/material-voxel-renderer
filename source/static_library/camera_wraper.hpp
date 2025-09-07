#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct camera_parameters
{
    glm::vec3 position; // 摄像机位置
    glm::vec3 forward;  // 摄像机前方向
    glm::vec3 up;       // 摄像机上方向
    glm::vec3 right;    // 摄像机右方向
    float fov;          // 摄像机视野
    float aspect;       // 视口宽高比
    float near_plane;   // 近裁剪面
    float far_plane;    // 远裁剪面
    bool orthographic;  // 是否为正交投影
};
// constexpr size_t camera_parameters_size = sizeof(camera_parameters); // 68 bytes
struct shader_camera
{
    glm::mat4 view;    // 视图矩阵
    glm::vec4 eye;     // 摄像机位置
    float view_plane;  // 视平面距离
    float fov;         // 摄像机视野
    float aspect;      // 视口宽高比
    bool orthographic; // 是否为正交投影
};
// constexpr size_t shader_camera_size = sizeof(shader_camera); // 96 bytes
struct has_changed
{
    bool changed = false;
    bool operator()()
    {
        if (changed)
            return changed = false, true;
        return false;
    }
    void mark_changed() { changed = true; }
};
class camera_wraper : public has_changed
{
public:
    camera_wraper() { reset(); }
    void reset()
    {
        params.position = { 0.0f, 0.0f, 5.0f };
        params.forward = { 0.0f, 0.0f, -1.0f };
        params.up = { 0.0f, 1.0f, 0.0f };
        params.right = glm::cross(params.forward, params.up);
        params.fov = glm::radians(45.0f);
        params.aspect = 4.0f / 3.0f;
        params.near_plane = 0.1f;
        params.far_plane = 100.0f;
        params.orthographic = false;
        mark_changed();
    }
    void set_orthographic(bool ortho) { params.orthographic = ortho; }
    void set_orientation(const glm::vec3& forward, const glm::vec3& up)
    {
        params.forward = glm::normalize(forward);
        params.up = glm::normalize(up);
        params.right = glm::normalize(glm::cross(params.forward, params.up));
        params.up = glm::cross(params.right, params.forward); // Re-orthogonalize up
    }
    void update_orientation(int mouse_dx, int mouse_dy, int8_t mouse_wheel)
    {
        if (mouse_dx != 0 || mouse_dy != 0)
        {
            float sensitivity = 0.005f;
            float yaw = -mouse_dx * sensitivity;
            float pitch = -mouse_dy * sensitivity;

            // Yaw rotation around the up vector
            glm::mat4 yaw_matrix = glm::rotate(glm::mat4(1.0f), yaw, params.up);
            params.forward = glm::normalize(glm::vec3(yaw_matrix * glm::vec4(params.forward, 0.0f)));
            params.right = glm::normalize(glm::cross(params.forward, params.up));

            // Pitch rotation around the right vector
            glm::mat4 pitch_matrix = glm::rotate(glm::mat4(1.0f), pitch, params.right);
            params.forward = glm::normalize(glm::vec3(pitch_matrix * glm::vec4(params.forward, 0.0f)));
            params.up = glm::normalize(glm::cross(params.right, params.forward));

            mark_changed();
        }
        if (mouse_wheel != 0)
        {
            float zoom_sensitivity = 0.1f;
            params.fov -= mouse_wheel * zoom_sensitivity;
            params.fov = glm::clamp(params.fov, glm::radians(10.0f), glm::radians(120.0f));
            mark_changed();
        }
    }

    shader_camera compute_shader_camera()
    {
        shader_camera sc;
        sc.view = glm::lookAt(params.position, params.position + params.forward, params.up);
        sc.eye = glm::vec4(params.position, 1.0f);
        sc.view_plane = params.near_plane;
        sc.fov = params.fov;
        sc.aspect = params.aspect;
        sc.orthographic = params.orthographic;
        return sc;
    }

    camera_parameters params;
};