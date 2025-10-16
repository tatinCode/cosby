# Cosby - osu! Storyboard Creator

A C++ application for creating osu! storyboards programmatically. This tool allows you to generate `.osb` (osu! storyboard) files with sprites, animations, and various commands like fading, moving, scaling, rotating, and color changes.

## Features

- **Sprite Management**: Create and manage sprites with different layers and origins
- **Animation Support**: Create animated sprites with frame-based animations
- **Command System**: Add various commands to sprites:
  - Fade: Control opacity over time
  - Move: Animate position changes
  - Scale: Resize sprites
  - Rotate: Rotate sprites
  - Color: Change sprite colors
- **Easing Functions**: Support for 35 different easing types (Linear, Quad, Cubic, etc.)
- **Layer System**: Organize sprites across 5 layers (Background, Fail, Pass, Foreground, Overlay)
- **CLI Interface**: Simple command-line interface for generating storyboards

## Building

### Requirements
- CMake 3.10 or higher
- C++17 compatible compiler (GCC, Clang, MSVC)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/tatinCode/cosby.git
cd cosby

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make

# The executable will be in build/bin/cosby
```

## Usage

### Command Line Options

```bash
cosby [options]

Options:
  -h, --help     Show help message
  -e, --example  Generate an example storyboard
  -o <file>      Output file (default: storyboard.osb)
```

### Quick Start

Generate an example storyboard:

```bash
./build/bin/cosby --example -o example.osb
```

### Programming with Cosby

Here's a simple example of creating a storyboard programmatically:

```cpp
#include "storyboard.h"
#include "sprite.h"
#include "command.h"

using namespace cosby;

int main() {
    // Create a storyboard
    Storyboard storyboard;
    
    // Create a sprite
    auto sprite = std::make_shared<Sprite>(
        Layer::Background,
        Origin::Centre,
        "myimage.png",
        320.0, 240.0  // x, y position
    );
    
    // Add commands to the sprite
    // Fade in from 0 to 1 opacity between 0ms and 1000ms
    sprite->addCommand(std::make_shared<FadeCommand>(
        Easing::Linear, 0, 1000, 0.0, 1.0
    ));
    
    // Move from (320, 240) to (400, 300) between 1000ms and 3000ms
    sprite->addCommand(std::make_shared<MoveCommand>(
        Easing::Out, 1000, 3000, 320.0, 240.0, 400.0, 300.0
    ));
    
    // Add sprite to storyboard
    storyboard.addSprite(sprite);
    
    // Save to file
    storyboard.saveToFile("mystoryboard.osb");
    
    return 0;
}
```

## Storyboard Format

The generated `.osb` files follow the osu! storyboard format:

- **Sprites**: Static images positioned on screen
- **Animations**: Multi-frame animated images
- **Commands**: Transformations applied to sprites over time
- **Layers**: Organizational layers for visual ordering
- **Origins**: Anchor points for sprite positioning

### Supported Commands

| Command | Description | Parameters |
|---------|-------------|------------|
| F (Fade) | Control opacity | startOpacity, endOpacity |
| M (Move) | Move sprite | startX, startY, endX, endY |
| S (Scale) | Scale sprite | startScale, endScale |
| R (Rotate) | Rotate sprite | startRotation, endRotation (radians) |
| C (Color) | Change color | startR, startG, startB, endR, endG, endB |

### Easing Types

Cosby supports 35 easing functions including:
- Linear
- Quadratic (In, Out, InOut)
- Cubic (In, Out, InOut)
- Quartic (In, Out, InOut)
- Quintic (In, Out, InOut)
- Sinusoidal (In, Out, InOut)
- Exponential (In, Out, InOut)
- Circular (In, Out, InOut)
- Elastic (In, Out, HalfOut, QuarterOut, InOut)
- Back (In, Out, InOut)
- Bounce (In, Out, InOut)

## License

This project is open source and available for use in creating osu! storyboards.

## Contributing

Contributions are welcome! Feel free to submit issues or pull requests.

## About osu!

osu! is a free-to-play rhythm game primarily developed, published and created by Dean "peppy" Herbert. For more information, visit [osu.ppy.sh](https://osu.ppy.sh/).

## References

- [osu! Storyboard Scripting](https://osu.ppy.sh/wiki/en/Storyboard/Scripting)
- [osu! File Formats](https://osu.ppy.sh/wiki/en/Client/File_formats)
