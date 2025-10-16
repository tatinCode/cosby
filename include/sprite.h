#ifndef SPRITE_H
#define SPRITE_H

#include <string>
#include <vector>
#include <memory>
#include "command.h"

namespace cosby {

// Layer types for sprites
enum class Layer {
    Background = 0,
    Fail = 1,
    Pass = 2,
    Foreground = 3,
    Overlay = 4
};

// Origin types for sprites
enum class Origin {
    TopLeft = 0,
    Centre = 1,
    CentreLeft = 2,
    TopRight = 3,
    BottomCentre = 4,
    TopCentre = 5,
    CentreRight = 7,
    BottomLeft = 8,
    BottomRight = 9
};

// Base sprite class
class Sprite {
public:
    Sprite(Layer layer, Origin origin, const std::string& filepath, double x, double y);
    virtual ~Sprite() = default;
    
    // Add commands
    void addCommand(std::shared_ptr<Command> command);
    
    // Generate storyboard output
    virtual std::string toString() const;
    
    // Getters
    Layer getLayer() const { return layer_; }
    Origin getOrigin() const { return origin_; }
    const std::string& getFilepath() const { return filepath_; }
    
protected:
    Layer layer_;
    Origin origin_;
    std::string filepath_;
    double x_, y_;
    std::vector<std::shared_ptr<Command>> commands_;
    
    std::string layerToString() const;
    std::string originToString() const;
};

// Animation sprite (for animated images)
class Animation : public Sprite {
public:
    Animation(Layer layer, Origin origin, const std::string& filepath, 
              double x, double y, int frameCount, int frameDelay, 
              const std::string& loopType = "LoopForever");
    
    std::string toString() const override;
    
private:
    int frameCount_;
    int frameDelay_;
    std::string loopType_;
};

} // namespace cosby

#endif // SPRITE_H
