#include "events.h"
#include "raylib.h"
#include <event2/event.h>
#include <vector>
#include <string>
#include <cmath>

#include "container.h"
#include "event.h"

#define dpi 1.6f

int main()
{
    const int screenWidth = 1400;
    const int screenHeight = 1200;

    //SetConfigFlags(EVENT_BASE_FLAG_IGNORE_ENV);
    
    InitWindow(screenWidth, screenHeight, "Container debugger");
	SetTargetFPS(165);

    static float bottomSplit = .9;
    static float rightSplit = .6;
    static float rightBottomSplit = .55;
    
    #define paint [](Container *root, Container *c)
    Container *root = new Container(::hbox, FILL_SPACE, FILL_SPACE);
    root->pre_layout = [](Container *root, Container *c, const Bounds &b) {
      c->children[1]->wanted_bounds.w = b.w - (b.w * rightSplit);
    };

    if (auto left = root->child(::vbox, FILL_SPACE, FILL_SPACE)) {
        left->pre_layout = [](Container *root, Container *c, const Bounds &b) {
            c->children[1]->wanted_bounds.h = b.h - (b.h * bottomSplit);
            if (c->children[1]->wanted_bounds.h < 50)
                c->children[1]->wanted_bounds.h = 50;
        };
        auto top = left->child(FILL_SPACE, FILL_SPACE);
        top->when_paint = paint {
          DrawRectangle(c->real_bounds.x, c->real_bounds.y, c->real_bounds.w,
                        c->real_bounds.h, GRAY);
        };
        auto bottom = left->child(FILL_SPACE, 60);
        bottom->when_paint = paint {
          DrawRectangle(c->real_bounds.x, c->real_bounds.y, c->real_bounds.w,
                        c->real_bounds.h, DARKGRAY);
        };
    }
    if (auto right = root->child(400, FILL_SPACE)) {
        right->pre_layout = [](Container *root, Container *c, const Bounds &b) {
            c->children[1]->wanted_bounds.h = b.h - (b.h * rightBottomSplit);
        };
        
        auto top = right->child(FILL_SPACE, FILL_SPACE);
        top->when_paint = paint {
          DrawRectangle(c->real_bounds.x, c->real_bounds.y, c->real_bounds.w,
                        c->real_bounds.h, LIGHTGRAY);
        };
        auto bottom = right->child(FILL_SPACE, 500);
        bottom->when_paint = paint {
          DrawRectangle(c->real_bounds.x, c->real_bounds.y, c->real_bounds.w,
                        c->real_bounds.h, BLUE);
        };
    }

    while (!WindowShouldClose()) {
      BeginDrawing();

      auto sw = GetScreenWidth();
      auto sh = GetScreenHeight();
      root->wanted_bounds = Bounds(0, 0, sw, sh);
      root->real_bounds = root->wanted_bounds;
      
      root->pre_layout(root, root, root->real_bounds);
      ::layout(root, root, root->real_bounds);

      auto m = GetMousePosition();

      paint_root(root);

      EndDrawing();
    }

    CloseWindow();

    return 0;
}
