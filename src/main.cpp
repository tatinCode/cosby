#include <iostream>
#include <memory>
#include "storyboard.h"
#include "sprite.h"
#include "command.h"

using namespace cosby;

void printUsage(const char* programName) {
    std::cout << "Cosby - osu! Storyboard Creator\n";
    std::cout << "================================\n\n";
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help     Show this help message\n";
    std::cout << "  -e, --example  Generate an example storyboard\n";
    std::cout << "  -o <file>      Output file (default: storyboard.osb)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " --example -o example.osb\n";
    std::cout << "  " << programName << " -e\n\n";
}

void generateExample(const std::string& outputFile) {
    Storyboard storyboard;
    
    // Create a sprite that fades in and out
    auto sprite1 = std::make_shared<Sprite>(
        Layer::Background,
        Origin::Centre,
        "sprite.png",
        320.0, 240.0
    );
    
    // Add fade in command (0ms to 1000ms, opacity 0 to 1)
    sprite1->addCommand(std::make_shared<FadeCommand>(
        Easing::Linear, 0, 1000, 0.0, 1.0
    ));
    
    // Add move command (1000ms to 3000ms)
    sprite1->addCommand(std::make_shared<MoveCommand>(
        Easing::Out, 1000, 3000, 320.0, 240.0, 400.0, 300.0
    ));
    
    // Add scale command (1000ms to 3000ms)
    sprite1->addCommand(std::make_shared<ScaleCommand>(
        Easing::QuadInOut, 1000, 3000, 1.0, 1.5
    ));
    
    // Add fade out command (3000ms to 4000ms)
    sprite1->addCommand(std::make_shared<FadeCommand>(
        Easing::Linear, 3000, 4000, 1.0, 0.0
    ));
    
    storyboard.addSprite(sprite1);
    
    // Create a second sprite with color and rotation
    auto sprite2 = std::make_shared<Sprite>(
        Layer::Foreground,
        Origin::Centre,
        "logo.png",
        100.0, 100.0
    );
    
    // Add color transition (red to blue)
    sprite2->addCommand(std::make_shared<ColorCommand>(
        Easing::Linear, 500, 2500, 255, 0, 0, 0, 0, 255
    ));
    
    // Add rotation
    sprite2->addCommand(std::make_shared<RotateCommand>(
        Easing::Linear, 500, 2500, 0.0, 6.28318  // 0 to 2*PI radians
    ));
    
    // Fade in and out
    sprite2->addCommand(std::make_shared<FadeCommand>(
        Easing::Linear, 500, 1000, 0.0, 1.0
    ));
    sprite2->addCommand(std::make_shared<FadeCommand>(
        Easing::Linear, 2000, 2500, 1.0, 0.0
    ));
    
    storyboard.addSprite(sprite2);
    
    // Create an animation example
    auto animation = std::make_shared<Animation>(
        Layer::Pass,
        Origin::Centre,
        "animation.png",
        320.0, 400.0,
        10,      // 10 frames
        100,     // 100ms per frame
        "LoopForever"
    );
    
    animation->addCommand(std::make_shared<FadeCommand>(
        Easing::Linear, 1000, 1500, 0.0, 1.0
    ));
    
    animation->addCommand(std::make_shared<FadeCommand>(
        Easing::Linear, 3500, 4000, 1.0, 0.0
    ));
    
    storyboard.addSprite(animation);
    
    // Save to file
    if (storyboard.saveToFile(outputFile)) {
        std::cout << "Example storyboard generated successfully!\n";
        std::cout << "Output file: " << outputFile << "\n";
        std::cout << "Total sprites: " << storyboard.getSpriteCount() << "\n\n";
        std::cout << "Generated content:\n";
        std::cout << "==================\n";
        std::cout << storyboard.generate();
    } else {
        std::cerr << "Error: Failed to write to file '" << outputFile << "'\n";
    }
}

int main(int argc, char* argv[]) {
    std::string outputFile = "storyboard.osb";
    bool generateExampleFlag = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-e" || arg == "--example") {
            generateExampleFlag = true;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            } else {
                std::cerr << "Error: -o requires a filename argument\n";
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (generateExampleFlag) {
        generateExample(outputFile);
    } else {
        printUsage(argv[0]);
        std::cout << "Tip: Use -e or --example to generate an example storyboard\n";
    }
    
    return 0;
}
