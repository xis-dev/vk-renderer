
#include "engine.h"

#define WIDTH 800
#define HEIGHT 450
int main()
{
    Engine engine;

    engine.init(WIDTH, HEIGHT);

    engine.run();

    engine.cleanup();

    return 0;
}