#include "storyboard.h"
#include <fstream>
#include <sstream>

namespace cosby {

Storyboard::Storyboard() {
}

void Storyboard::addSprite(std::shared_ptr<Sprite> sprite) {
    sprites_.push_back(sprite);
}

std::string Storyboard::generate() const {
    std::ostringstream oss;
    oss << "[Events]\n";
    oss << "//Background and Video events\n";
    oss << "//Storyboard Layer 0 (Background)\n";
    oss << "//Storyboard Layer 1 (Fail)\n";
    oss << "//Storyboard Layer 2 (Pass)\n";
    oss << "//Storyboard Layer 3 (Foreground)\n";
    oss << "//Storyboard Layer 4 (Overlay)\n";
    oss << "//Storyboard Sound Samples\n";
    
    for (const auto& sprite : sprites_) {
        oss << sprite->toString();
    }
    
    return oss.str();
}

bool Storyboard::saveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << generate();
    file.close();
    return true;
}

void Storyboard::clear() {
    sprites_.clear();
}

} // namespace cosby
