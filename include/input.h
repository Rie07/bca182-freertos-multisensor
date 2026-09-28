#ifndef INPUT_H
#define INPUT_H

enum class DisplayMode
{
    TEMPERATURE,
    HUMIDITY,
    LIGHT,
    MOTION
};

DisplayMode nextDisplayMode(DisplayMode currentMode);

DisplayMode previousDisplayMode(DisplayMode currentMode);

#endif