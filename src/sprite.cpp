#include "sprite.h"
#include <sstream>

namespace cosby {

Sprite::Sprite(Layer layer, Origin origin, const std::string& filepath, double x, double y)
    : layer_(layer), origin_(origin), filepath_(filepath), x_(x), y_(y) {
}

void Sprite::addCommand(std::shared_ptr<Command> command) {
    commands_.push_back(command);
}

std::string Sprite::layerToString() const {
    switch (layer_) {
        case Layer::Background: return "Background";
        case Layer::Fail: return "Fail";
        case Layer::Pass: return "Pass";
        case Layer::Foreground: return "Foreground";
        case Layer::Overlay: return "Overlay";
        default: return "Background";
    }
}

std::string Sprite::originToString() const {
    switch (origin_) {
        case Origin::TopLeft: return "TopLeft";
        case Origin::Centre: return "Centre";
        case Origin::CentreLeft: return "CentreLeft";
        case Origin::TopRight: return "TopRight";
        case Origin::BottomCentre: return "BottomCentre";
        case Origin::TopCentre: return "TopCentre";
        case Origin::CentreRight: return "CentreRight";
        case Origin::BottomLeft: return "BottomLeft";
        case Origin::BottomRight: return "BottomRight";
        default: return "Centre";
    }
}

std::string Sprite::toString() const {
    std::ostringstream oss;
    oss << "Sprite," << layerToString() << "," << originToString() << ",\"" 
        << filepath_ << "\"," << x_ << "," << y_ << "\n";
    
    for (const auto& command : commands_) {
        oss << command->toString() << "\n";
    }
    
    return oss.str();
}

// Animation implementation
Animation::Animation(Layer layer, Origin origin, const std::string& filepath,
                     double x, double y, int frameCount, int frameDelay,
                     const std::string& loopType)
    : Sprite(layer, origin, filepath, x, y),
      frameCount_(frameCount), frameDelay_(frameDelay), loopType_(loopType) {
}

std::string Animation::toString() const {
    std::ostringstream oss;
    oss << "Animation," << layerToString() << "," << originToString() << ",\"" 
        << filepath_ << "\"," << x_ << "," << y_ << ","
        << frameCount_ << "," << frameDelay_ << "," << loopType_ << "\n";
    
    for (const auto& command : commands_) {
        oss << command->toString() << "\n";
    }
    
    return oss.str();
}

} // namespace cosby
