#ifndef COMMAND_H
#define COMMAND_H

#include <string>
#include <vector>

namespace cosby {

// Easing types for commands
enum class Easing {
    Linear = 0,
    Out = 1,
    In = 2,
    QuadIn = 3,
    QuadOut = 4,
    QuadInOut = 5,
    CubicIn = 6,
    CubicOut = 7,
    CubicInOut = 8,
    QuartIn = 9,
    QuartOut = 10,
    QuartInOut = 11,
    QuintIn = 12,
    QuintOut = 13,
    QuintInOut = 14,
    SineIn = 15,
    SineOut = 16,
    SineInOut = 17,
    ExpoIn = 18,
    ExpoOut = 19,
    ExpoInOut = 20,
    CircIn = 21,
    CircOut = 22,
    CircInOut = 23,
    ElasticIn = 24,
    ElasticOut = 25,
    ElasticHalfOut = 26,
    ElasticQuarterOut = 27,
    ElasticInOut = 28,
    BackIn = 29,
    BackOut = 30,
    BackInOut = 31,
    BounceIn = 32,
    BounceOut = 33,
    BounceInOut = 34
};

// Base command class
class Command {
public:
    Command(const std::string& type, Easing easing, int startTime, int endTime);
    virtual ~Command() = default;
    
    virtual std::string toString() const = 0;
    
protected:
    std::string type_;
    Easing easing_;
    int startTime_;
    int endTime_;
};

// Fade command
class FadeCommand : public Command {
public:
    FadeCommand(Easing easing, int startTime, int endTime, double startOpacity, double endOpacity);
    std::string toString() const override;
    
private:
    double startOpacity_;
    double endOpacity_;
};

// Move command
class MoveCommand : public Command {
public:
    MoveCommand(Easing easing, int startTime, int endTime, 
                double startX, double startY, double endX, double endY);
    std::string toString() const override;
    
private:
    double startX_, startY_;
    double endX_, endY_;
};

// Scale command
class ScaleCommand : public Command {
public:
    ScaleCommand(Easing easing, int startTime, int endTime, double startScale, double endScale);
    std::string toString() const override;
    
private:
    double startScale_;
    double endScale_;
};

// Rotate command
class RotateCommand : public Command {
public:
    RotateCommand(Easing easing, int startTime, int endTime, double startRotation, double endRotation);
    std::string toString() const override;
    
private:
    double startRotation_;
    double endRotation_;
};

// Color command
class ColorCommand : public Command {
public:
    ColorCommand(Easing easing, int startTime, int endTime,
                 int startR, int startG, int startB,
                 int endR, int endG, int endB);
    std::string toString() const override;
    
private:
    int startR_, startG_, startB_;
    int endR_, endG_, endB_;
};

} // namespace cosby

#endif // COMMAND_H
