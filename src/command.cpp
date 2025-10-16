#include "command.h"
#include <sstream>
#include <iomanip>

namespace cosby {

Command::Command(const std::string& type, Easing easing, int startTime, int endTime)
    : type_(type), easing_(easing), startTime_(startTime), endTime_(endTime) {
}

// FadeCommand implementation
FadeCommand::FadeCommand(Easing easing, int startTime, int endTime, 
                         double startOpacity, double endOpacity)
    : Command("F", easing, startTime, endTime), 
      startOpacity_(startOpacity), endOpacity_(endOpacity) {
}

std::string FadeCommand::toString() const {
    std::ostringstream oss;
    oss << "  " << type_ << "," << static_cast<int>(easing_) << "," 
        << startTime_ << "," << endTime_ << "," 
        << std::fixed << std::setprecision(2) << startOpacity_ << "," << endOpacity_;
    return oss.str();
}

// MoveCommand implementation
MoveCommand::MoveCommand(Easing easing, int startTime, int endTime,
                         double startX, double startY, double endX, double endY)
    : Command("M", easing, startTime, endTime),
      startX_(startX), startY_(startY), endX_(endX), endY_(endY) {
}

std::string MoveCommand::toString() const {
    std::ostringstream oss;
    oss << "  " << type_ << "," << static_cast<int>(easing_) << "," 
        << startTime_ << "," << endTime_ << "," 
        << std::fixed << std::setprecision(2)
        << startX_ << "," << startY_ << "," << endX_ << "," << endY_;
    return oss.str();
}

// ScaleCommand implementation
ScaleCommand::ScaleCommand(Easing easing, int startTime, int endTime, 
                           double startScale, double endScale)
    : Command("S", easing, startTime, endTime),
      startScale_(startScale), endScale_(endScale) {
}

std::string ScaleCommand::toString() const {
    std::ostringstream oss;
    oss << "  " << type_ << "," << static_cast<int>(easing_) << "," 
        << startTime_ << "," << endTime_ << "," 
        << std::fixed << std::setprecision(2) << startScale_ << "," << endScale_;
    return oss.str();
}

// RotateCommand implementation
RotateCommand::RotateCommand(Easing easing, int startTime, int endTime,
                             double startRotation, double endRotation)
    : Command("R", easing, startTime, endTime),
      startRotation_(startRotation), endRotation_(endRotation) {
}

std::string RotateCommand::toString() const {
    std::ostringstream oss;
    oss << "  " << type_ << "," << static_cast<int>(easing_) << "," 
        << startTime_ << "," << endTime_ << "," 
        << std::fixed << std::setprecision(2) << startRotation_ << "," << endRotation_;
    return oss.str();
}

// ColorCommand implementation
ColorCommand::ColorCommand(Easing easing, int startTime, int endTime,
                           int startR, int startG, int startB,
                           int endR, int endG, int endB)
    : Command("C", easing, startTime, endTime),
      startR_(startR), startG_(startG), startB_(startB),
      endR_(endR), endG_(endG), endB_(endB) {
}

std::string ColorCommand::toString() const {
    std::ostringstream oss;
    oss << "  " << type_ << "," << static_cast<int>(easing_) << "," 
        << startTime_ << "," << endTime_ << "," 
        << startR_ << "," << startG_ << "," << startB_ << ","
        << endR_ << "," << endG_ << "," << endB_;
    return oss.str();
}

} // namespace cosby
