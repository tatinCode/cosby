# Example: Creating a Custom Storyboard

This example shows how to create a custom storyboard using the Cosby library.

## Simple Example

```cpp
#include "storyboard.h"
#include "sprite.h"
#include "command.h"
#include <memory>

using namespace cosby;

int main() {
    // Create a new storyboard
    Storyboard sb;
    
    // Create a sprite
    auto mySprite = std::make_shared<Sprite>(
        Layer::Background,      // Layer
        Origin::Centre,          // Origin point
        "background.jpg",        // Image file path
        320.0, 240.0            // Position (x, y)
    );
    
    // Fade in from invisible to fully visible
    mySprite->addCommand(std::make_shared<FadeCommand>(
        Easing::Linear,    // Easing type
        0,                 // Start time (ms)
        1000,              // End time (ms)
        0.0,               // Start opacity (0 = invisible)
        1.0                // End opacity (1 = fully visible)
    ));
    
    // Add the sprite to storyboard
    sb.addSprite(mySprite);
    
    // Save the storyboard
    sb.saveToFile("mystoryboard.osb");
    
    return 0;
}
```

## Advanced Example

```cpp
#include "storyboard.h"
#include "sprite.h"
#include "command.h"
#include <memory>

using namespace cosby;

int main() {
    Storyboard sb;
    
    // Create an animated logo
    auto logo = std::make_shared<Sprite>(
        Layer::Foreground,
        Origin::Centre,
        "logo.png",
        320.0, 100.0
    );
    
    // Fade in
    logo->addCommand(std::make_shared<FadeCommand>(
        Easing::Out, 0, 500, 0.0, 1.0
    ));
    
    // Move down while scaling up
    logo->addCommand(std::make_shared<MoveCommand>(
        Easing::QuadOut, 500, 2000,
        320.0, 100.0,    // Start position
        320.0, 240.0     // End position
    ));
    
    logo->addCommand(std::make_shared<ScaleCommand>(
        Easing::QuadOut, 500, 2000,
        0.5,             // Start scale
        1.0              // End scale
    ));
    
    // Rotate 360 degrees (2*PI radians)
    logo->addCommand(std::make_shared<RotateCommand>(
        Easing::Linear, 2000, 4000,
        0.0,             // Start rotation
        6.28318          // End rotation (2*PI)
    ));
    
    // Color transition from white to blue
    logo->addCommand(std::make_shared<ColorCommand>(
        Easing::Linear, 2000, 4000,
        255, 255, 255,   // Start color (white)
        0, 100, 255      // End color (blue)
    ));
    
    // Fade out
    logo->addCommand(std::make_shared<FadeCommand>(
        Easing::In, 4000, 5000, 1.0, 0.0
    ));
    
    sb.addSprite(logo);
    
    // Create a background animation
    auto bgAnimation = std::make_shared<Animation>(
        Layer::Background,
        Origin::Centre,
        "particles.png",
        320.0, 240.0,
        30,                  // Frame count
        50,                  // Frame delay (ms)
        "LoopForever"        // Loop type
    );
    
    bgAnimation->addCommand(std::make_shared<FadeCommand>(
        Easing::Linear, 0, 500, 0.0, 0.5
    ));
    
    sb.addSprite(bgAnimation);
    
    // Save the storyboard
    sb.saveToFile("advanced_storyboard.osb");
    
    return 0;
}
```

## Building Your Custom Storyboard

1. Create your C++ file (e.g., `my_storyboard.cpp`)
2. Include the necessary headers
3. Link against the Cosby library:

```bash
g++ -std=c++17 my_storyboard.cpp \
    src/storyboard.cpp src/sprite.cpp src/command.cpp \
    -I include -o my_storyboard
```

4. Run your program:

```bash
./my_storyboard
```

## Tips

- **Timing**: All times are in milliseconds
- **Position**: Standard osu! coordinates (usually 320x240 for 4:3, or 640x480 for full resolution)
- **Opacity**: Range from 0.0 (invisible) to 1.0 (fully visible)
- **Rotation**: In radians (2*PI = 360 degrees)
- **Layers**: Use appropriate layers for proper visual ordering
- **Easing**: Choose easing functions that match your desired animation style

## Available Layers

- `Layer::Background` - Behind everything
- `Layer::Fail` - Shown during fails
- `Layer::Pass` - Shown during passes
- `Layer::Foreground` - In front of playfield
- `Layer::Overlay` - On top of everything

## Available Origins

- `Origin::TopLeft`, `Origin::TopCentre`, `Origin::TopRight`
- `Origin::CentreLeft`, `Origin::Centre`, `Origin::CentreRight`
- `Origin::BottomLeft`, `Origin::BottomCentre`, `Origin::BottomRight`
