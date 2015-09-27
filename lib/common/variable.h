#ifndef VARIABLE
#define VARIABLE

#include "macro.h"

enum cmdType
{
    START = 1,
    STOP,
    PAUSE,
    RESUME
};

struct Plane2DCoordinate
{
    Coordinate x;
    Coordinate y;
};

struct Spot3DCoordinate
{
    Coordinate x;
    Coordinate y;
    Coordinate z;
};

struct SpotSonicationParameter
{
    VOLT volt;
    int totalTime;
    int period;
    int dutyCycle;
    int coolingTime;
};

struct SessionRecorder
{
    int spotIndex;
    int periodIndex;
};

struct SessionParameter
{
    int spotCount;
    int periodCount;
    int dutyOn;
    int dutyOff;
    int coolingTime;
};

#endif // VARIABLE

