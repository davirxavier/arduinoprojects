// #include <Arduino.h>
// #include <math.h>
//
// #define IMAGE_W 1280.0f
// #define IMAGE_H 720.0f
//
// // Physical geometry
// #define CAMERA_HEIGHT   1.0f
// #define CAMERA_OFFSET_X 0.15f   // camera is 15 cm LEFT of pan axis
//
// // Servo limits
// #define PAN_MIN   20.0f
// #define PAN_MAX  180.0f
//
// #define TILT_MIN   0.0f
// #define TILT_MAX 120.0f
//
// // Camera FOV
// #define HFOV 160.0f
//
// // Adjust these for tilt calibration
// #define VFOV 120.0f
// #define TILT_OFFSET 0.0f
//
// static inline float deg(float r)
// {
//     return r * 180.0f / PI;
// }
//
// static inline float rad(float d)
// {
//     return d * PI / 180.0f;
// }
//
// void pixelToServo(float x, float y, float &pan, float &tilt)
// {
//     // Automatically calculate servo centers from limits
//     const float panCenter  = (PAN_MIN + PAN_MAX) * 0.5f;
//     const float tiltCenter = (TILT_MIN + TILT_MAX) * 0.5f;
//
//     // Image center
//     const float cx = IMAGE_W * 0.5f;
//     const float cy = IMAGE_H * 0.5f;
//
//     // ------------------------------------------------------------
//     // X / PAN
//     // ------------------------------------------------------------
//
//     const float fx =
//         (IMAGE_W * 0.5f) / tanf(rad(HFOV * 0.5f));
//
//     const float angleX =
//         atanf((x - cx) / fx);
//
//     // Convert camera angle to couch position
//     float groundX =
//         CAMERA_HEIGHT * tanf(angleX);
//
//     // Camera is 15 cm LEFT of pan axis
//     groundX -= CAMERA_OFFSET_X;
//
//     // Image RIGHT = LOWER pan angle
//     pan =
//         panCenter -
//         deg(atan2f(groundX, CAMERA_HEIGHT));
//
//     // ------------------------------------------------------------
//     // Y / TILT
//     // ------------------------------------------------------------
//
//     const float fy =
//         (IMAGE_H * 0.5f) / tanf(rad(VFOV * 0.5f));
//
//     const float angleY =
//         atanf((y - cy) / fy);
//
//     // Image DOWN = HIGHER tilt angle
//     tilt =
//         tiltCenter +
//         deg(angleY) +
//         TILT_OFFSET;
//
//     // ------------------------------------------------------------
//     // Clamp to servo limits
//     // ------------------------------------------------------------
//
//     pan = constrain(pan, PAN_MIN, PAN_MAX);
//     tilt = constrain(tilt, TILT_MIN, TILT_MAX);
// }

inline void pixelToServo(float x, float y, float &pan, float &tilt)
{
    pan = map(x, 0, 1100, 180, 20);
    tilt = map(y, 0, 600, 0, 120);
}