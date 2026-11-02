#include "Camera.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
: Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM) {
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    // 根据初始欧拉角计算 Front、Right、Up 向量
    updateCameraVectors();
}

// 根据初始欧拉角计算 Front、Right、Up 向量
Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) 
: Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM) {
    Position = glm::vec3(posX, posY, posZ);
    WorldUp = glm::vec3(upX, upY, upZ);
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

// 获取当前相机位置
glm::mat4 Camera::GetViewMatrix() {
    return glm::lookAt(Position, Position + Front, Up);
}

// 根据当前欧拉角重新计算 Front、Right、Up 向量
void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime) {
    float velocity = MovementSpeed * deltaTime; // 计算本帧移动距离
    if (direction == FORWARD)
        Position += Front * velocity;           // 沿朝向前进
    if (direction == BACKWARD)
        Position -= Front * velocity;           // 沿朝向后退
    if (direction == LEFT)
        Position -= Right * velocity;           // 向左平移（注意：-Right）
    if (direction == RIGHT)
        Position += Right * velocity;           // 向右平移
}

// 应用鼠标灵敏度缩放偏移量
void Camera::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    // 更新欧拉角
    Yaw   += xoffset;   // 左右旋转（偏航）
    Pitch += yoffset;   // 上下旋转（俯仰）

    // 限制俯仰角在 [-89°, 89°] 之间，避免万向节死锁和画面翻转
    if (constrainPitch)
    {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    // 根据新的欧拉角重新计算相机的方向向量
    updateCameraVectors();
}
// 根据当前 Zoom 值重新计算 FOV
void Camera::ProcessMouseScroll(float yoffset) {
    // 调整 Zoom（FOV）：滚轮向上（yoffset > 0）则 Zoom 减小（拉近）
    Zoom -= (float)yoffset;
    // 限制 Zoom 范围（1° ~ 45°），避免极端视角
    if (Zoom < 1.0f)
        Zoom = 1.0f;
    if (Zoom > 45.0f)
        Zoom = 45.0f;
}

void Camera::updateCameraVectors() {
    // 使用球坐标公式计算新的 Front 向量
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch)); // X 分量
    front.y = sin(glm::radians(Pitch));                          // Y 分量（仅由俯仰决定）
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch)); // Z 分量
    Front = glm::normalize(front); // 归一化为单位向量

    // 重新计算 Right 向量：Front × WorldUp（右手坐标系）
    Right = glm::normalize(glm::cross(Front, WorldUp));

    // 重新计算实际的 Up 向量：Right × Front（确保 Up 垂直于观察平面）
    Up = glm::normalize(glm::cross(Right, Front));
}