#ifndef STORYBOARD_H
#define STORYBOARD_H

#include <string>
#include <vector>
#include <memory>
#include "sprite.h"

namespace cosby {

class Storyboard {
public:
    Storyboard();
    ~Storyboard() = default;
    
    // Add sprites to storyboard
    void addSprite(std::shared_ptr<Sprite> sprite);
    
    // Generate .osb file content
    std::string generate() const;
    
    // Save to file
    bool saveToFile(const std::string& filename) const;
    
    // Clear all sprites
    void clear();
    
    // Get sprite count
    size_t getSpriteCount() const { return sprites_.size(); }
    
private:
    std::vector<std::shared_ptr<Sprite>> sprites_;
};

} // namespace cosby

#endif // STORYBOARD_H
