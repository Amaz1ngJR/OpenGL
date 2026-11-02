#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


// 定义相机移动方向的枚举类型
// 用于抽象键盘输入，避免直接依赖具体窗口系统的按键处理
enum Camera_Movement {
    FORWARD,   // 向前
    BACKWARD,  // 向后
    LEFT,      // 向左
    RIGHT      // 向右
};

// 默认相机参数（全局常量）
const float YAW         = -90.0f;   // 偏航角（Yaw）：绕 Y 轴旋转，-90 表示初始朝向 -Z 方向（OpenGL 习惯）
const float PITCH       = 0.0f;     // 俯仰角（Pitch）：绕 X 轴旋转，0 表示水平平视
const float SPEED       = 2.5f;     // 相机移动速度（单位/秒）
const float SENSITIVITY = 0.1f;     // 鼠标灵敏度（控制旋转速度）
const float ZOOM        = 45.0f;    // 初始视野（FOV，单位：度）

class Camera
{
public:
	// ===== 相机的基本属性 =====
    glm::vec3 Position;    // 相机在世界坐标系中的位置
    glm::vec3 Front;       // 相机朝向（从 Position 指向目标点的方向向量）
    glm::vec3 Up;          // 相机的“上”方向（通常接近世界 Y 轴，但会随旋转变化）
    glm::vec3 Right;       // 相机的“右”方向（由 Front 和 WorldUp 叉乘得到）
    glm::vec3 WorldUp;     // 世界坐标系的“上”方向（通常为 (0,1,0)，用于计算 Right 和 Up）
	// ===== 欧拉角（用于控制视角）=====
    float Yaw;             // 偏航角（左右旋转）
    float Pitch;           // 俯仰角（上下抬头/低头）
	// ===== 相机行为参数 =====
    float MovementSpeed;   // 移动速度（可动态调整）
    float MouseSensitivity;// 鼠标灵敏度（可动态调整）
    float Zoom;            // 视野角度（FOV，用于投影矩阵，值越小“拉得越近”）

	// 构造函数 使用向量初始化
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH);
    // 构造函数 使用标量初始化
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

    // ===== 获取 View 矩阵 =====
    // View 矩阵用于将世界坐标转换到相机（观察）坐标系
    // 使用 GLM 的 lookAt 函数：从 Position 看向 Position + Front，以 Up 为上方向
    glm::mat4 GetViewMatrix();

    // ===== 处理键盘输入（WASD 等）=====
    // direction: 移动方向（FORWARD/BACKWARD/LEFT/RIGHT）
    // deltaTime: 上一帧到当前帧的时间差（用于帧率无关移动）
    void ProcessKeyboard(Camera_Movement direction, float deltaTime);

    // ===== 处理鼠标移动=====
    // xoffset / yoffset: 鼠标在 X/Y 方向的偏移量（通常由回调函数传入）
    // constrainPitch: 是否限制俯仰角（防止翻转）
    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true);

    // ===== 处理鼠标滚轮（缩放视野）=====
    // yoffset: 滚轮在垂直方向的偏移（向上为正，向下为负）
    void ProcessMouseScroll(float yoffset);

private:
	// ===== 根据欧拉角更新相机的方向向量 =====
    // 将 Yaw/Pitch 转换为单位方向向量 Front （同时更新 Right 和 Up）
    void updateCameraVectors();
};