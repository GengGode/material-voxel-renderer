#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>

struct camera_info
{
    // 摄像机位置
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    // 摄像机四元数
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    // 摄像机视点距离
    float target_distance = 3.0f;
    // 摄像机视角立体角
    float fov = glm::radians(45.0f);
    // 摄像机宽高比
    float aspect_ratio = 4.0f / 3.0f;
    // 摄像机前方向
    glm::vec3 front() const { return orientation * glm::vec3(0.0f, 0.0f, -1.0f); }
    // 摄像机上方向
    glm::vec3 up() const { return orientation * glm::vec3(0.0f, 1.0f, 0.0f); }
    // 摄像机右方向
    glm::vec3 right() const { return orientation * glm::vec3(1.0f, 0.0f, 0.0f); }
    // 摄像机lookAt矩阵
    glm::mat4 view() const { return glm::lookAt(position, position + front(), up()); }
    // 摄像机投影矩阵
    glm::mat4 projection() const { return glm::perspective(fov, aspect_ratio, 0.1f, 1000.0f); }
    // 通过鼠标移动摄像机位置
    void rotate_from_mouse(int mouse_dx, int mouse_dy, float sensitivity = 0.005f)
    {
        float yaw = -static_cast<float>(mouse_dx) * sensitivity;
        float pitch = -static_cast<float>(mouse_dy) * sensitivity;

        glm::quat yaw_quat = glm::angleAxis(yaw, up());
        glm::quat pitch_quat = glm::angleAxis(pitch, right());

        orientation = glm::normalize(yaw_quat * pitch_quat * orientation);
    }
    void orbit_around_target(int mouse_dx, int mouse_dy, float sensitivity = 0.005f)
    {
        // 先计算目标点
        glm::vec3 target_point = position + front() * target_distance;
        rotate_from_mouse(mouse_dx, mouse_dy, sensitivity);
        position = target_point - front() * target_distance;
    }
    // 通过鼠标滚轮缩放摄像机位置
    void zoom_from_scroll(int scroll_offset, float sensitivity = 0.05f)
    {
        target_distance -= static_cast<float>(scroll_offset) * sensitivity;
        if (target_distance < 0.1f)
            target_distance = 0.1f;
        position = position + front() * (static_cast<float>(scroll_offset) * sensitivity);
    }
    // 通过鼠标滚轮缩放调整fov
    void adjust_fov(int scroll_offset, float sensitivity = 0.05f)
    {
        fov -= static_cast<float>(scroll_offset) * sensitivity;
        fov = glm::clamp(fov, glm::radians(1.0f), glm::radians(90.0f));
    }
    // 设置目标点
    void set_target(const glm::vec3& target, bool maintain_distance = true)
    {
        if (glm::length(target - position) < 0.001f)
            return;
        if (maintain_distance)
            target_distance = glm::length(target - position);
        look_at(target);
    }
    // 看向特定点
    void look_at(const glm::vec3& target)
    {
        glm::vec3 direction = glm::normalize(target - position);
        orientation = glm::quatLookAt(direction, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    struct status
    {
        double last_mouse_x = 0.0;
        double last_mouse_y = 0.0;
        bool is_rotating = false;
        bool is_orbiting = false;
        bool is_zooming = false;
        float move_sensitivity = 0.005f;
        float scroll_sensitivity = 0.05f;
        float zoom_sensitivity = 0.05f;
        void reset_flags()
        {
            last_mouse_x = 0.0;
            last_mouse_y = 0.0;
            is_rotating = false;
            is_orbiting = false;
            is_zooming = false;
        }
        void mouse_controls(ImGuiIO& io, camera_info& cam)
        {
            if (io.WantCaptureMouse)
                return reset_flags();

            double current_mouse_x = io.MousePos.x;
            double current_mouse_y = io.MousePos.y;
            double wheel = io.MouseWheel;

            bool left_pressed = io.MouseDown[0];
            bool right_pressed = io.MouseDown[1];
            bool middle_pressed = io.MouseDown[2];

            if (right_pressed)
            {
                if (is_rotating)
                {
                    int dx = static_cast<int>(current_mouse_x - last_mouse_x);
                    int dy = static_cast<int>(current_mouse_y - last_mouse_y);
                    cam.rotate_from_mouse(dx, dy, move_sensitivity);
                }
                is_rotating = true;
            }
            else
            {
                is_rotating = false;
                // SPDLOG_INFO("Stop rotating");
            }

            if (left_pressed)
            {
                if (is_orbiting)
                {
                    int dx = static_cast<int>(current_mouse_x - last_mouse_x);
                    int dy = static_cast<int>(current_mouse_y - last_mouse_y);
                    cam.orbit_around_target(dx, dy, move_sensitivity);
                }
                is_orbiting = true;
            }
            else
            {
                is_orbiting = false;
                // SPDLOG_INFO("Stop orbiting");
            }

            if (middle_pressed)
            {
                if (is_zooming)
                {
                    int dy = static_cast<int>(current_mouse_y - last_mouse_y);
                    cam.zoom_from_scroll(-dy, scroll_sensitivity);
                }
                is_zooming = true;
            }
            else
            {
                is_zooming = false;
                // SPDLOG_INFO("Stop zooming");
            }

            last_mouse_x = current_mouse_x;
            last_mouse_y = current_mouse_y;

            if (wheel != 0.0f)
            {
                cam.adjust_fov(static_cast<int>(-wheel), zoom_sensitivity);
            }
        }
        void keyboard_controls(ImGuiIO& io, camera_info& cam)
        {
#if 0
            if (io.WantCaptureKeyboard)
                return reset_flags();

            float delta_time = io.DeltaTime;
            float velocity = move_sensitivity * delta_time;

            float move_speed = 0.1f;
            if (io.KeyShift)
                move_speed *= 2.0f;
            if (io.KeyCtrl)
                move_speed *= 0.5f;

            if (io.KeysData[ImGuiKey_W].Down)
                cam.position += cam.front() * move_speed;
            if (io.KeysData[ImGuiKey_S].Down)
                cam.position -= cam.front() * move_speed;
            if (io.KeysData[ImGuiKey_A].Down)
                cam.position -= cam.right() * move_speed;
            if (io.KeysData[ImGuiKey_D].Down)
                cam.position += cam.right() * move_speed;
            if (io.KeysData[ImGuiKey_Q].Down)
                cam.position -= cam.up() * move_speed;
            if (io.KeysData[ImGuiKey_E].Down)
                cam.position += cam.up() * move_speed;
#else
            (void)io;
            (void)cam;
#endif
        }
    } status;
};