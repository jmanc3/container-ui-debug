#include "events.h"
#include "raylib.h"
#include <cmath>
#include <event2/event.h>
#include <fstream>
#include <string>
#include <vector>

#include "container.h"
#include "event.h"
#include "json.hpp"

#define dpi 1.6f
#define paint [](Container * root, Container * c)

static std::vector<Container *> roots;
static Container *clicked = nullptr;
static int total_steps = 0;
static int current_step = 0;
static float zoom_factor = 1.0;
static float plane_x_off = 0.0;
static float plane_y_off = 0.0;

Container *import_container(const nlohmann::json &j) {
  auto *c = new Container();

  c->uuid = j.value("id", "");

  c->real_bounds.x = j.value("x", 0);
  c->real_bounds.y = j.value("y", 0);
  c->real_bounds.w = j.value("w", 0);
  c->real_bounds.h = j.value("h", 0);

  c->active = j.value("active", false);
  c->state.concerned = j.value("concerned", false);
  c->exists = j.value("exists", false);
  c->state.mouse_hovering = j.value("mouse_hovering", false);
  c->state.mouse_pressing = j.value("mouse_pressing", false);
  c->state.mouse_dragging = j.value("mouse_dragging", false);
  c->state.mouse_button_pressed = j.value("mouse_button_pressed", 0);

  c->spacing = j.value("spacing", 0);
  c->scroll_h_real = j.value("scroll_h_real", 0);
  c->scroll_v_real = j.value("scroll_v_real", 0);

  // children
  if (j.contains("children")) {
    for (const auto &child_json : j["children"]) {
      Container *child = import_container(child_json);
      c->children.push_back(child);
    }
  }

  return c;
}

// depth=0 → white
// deeper → darker gray
static Color DepthColor(int depth) {
  // Clamp depth so it never goes negative or too dark
  int shade = 255 - depth * 30;
  if (shade < 40)
    shade = 40; // minimum brightness
  return Color{(unsigned char)shade, (unsigned char)shade, (unsigned char)shade,
               255};
}

void paint_active_root(Container *root, Container *c, float zoom, float x_off,
                       float y_off, int depth) {
  if (!c)
    return;

  Color col = DepthColor(depth);

  // Apply zoom + offsets
  int x = (int)(c->real_bounds.x * zoom + x_off);
  int y = (int)(c->real_bounds.y * zoom + y_off);
  int w = (int)(c->real_bounds.w * zoom);
  int h = (int)(c->real_bounds.h * zoom);

  if (c == clicked) {
      col.r = 1.0;
  }
  DrawRectangle(x, y, w, h, col);

  for (auto *child : c->children) {
    paint_active_root(root, child, zoom, x_off, y_off, depth + 1);
  }
}

void paint_active_root(Container *root, Container *c) {
  auto debug_root = roots[current_step];
  paint_active_root(debug_root, debug_root, zoom_factor, plane_x_off,
                    plane_y_off, 0);
}

void select_container() {
  auto m = GetMousePosition();

  // Deproject click 
  m.x *= 1.0 / zoom_factor;
  m.y *= 1.0 / zoom_factor;
  m.x -= plane_x_off * (1 / zoom_factor);
  m.y -= plane_y_off * (1 / zoom_factor);

  auto p = pierced_containers(roots[current_step], m.x, m.y);
  if (!p.empty()) {
      clicked = p[0];
  } else {
      clicked = nullptr;
  }
}

int main() {
  const int screenWidth = 1400;
  const int screenHeight = 1200;

  // SetConfigFlags(EVENT_BASE_FLAG_IGNORE_ENV);

  InitWindow(screenWidth, screenHeight, "Container debugger");
  SetTargetFPS(165);

  static float bottomSplit = .91;
  static float rightSplit = .6;
  static float rightBottomSplit = .55;

  std::ifstream file("/home/jmanc3/Projects/containerdebug/log.json");
  std::string line;

  while (std::getline(file, line)) {
    nlohmann::json ex1 = nlohmann::json::parse(line);
    Container *root = import_container(ex1);
    roots.push_back(root);

    // printf("%s\n", ex1.dump(4).c_str());
  }

  total_steps = roots.size();
  current_step = 0;

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

      paint_active_root(root, c);
    };
    auto bottom = left->child(FILL_SPACE, 60);
    bottom->when_paint = paint {
      DrawRectangle(c->real_bounds.x, c->real_bounds.y, c->real_bounds.w,
                    c->real_bounds.h, DARKGRAY);
      DrawText(std::to_string(current_step).c_str(), c->real_bounds.x, c->real_bounds.y, 20 * dpi, BLACK);
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
    if (IsKeyDown(KEY_RIGHT)) {
      current_step++;
      if (current_step > roots.size() - 1)
          current_step = roots.size() - 1;
    }

    if (IsKeyDown(KEY_LEFT)) {
      current_step--;
      if (current_step < 0)
          current_step = 0;
    }

    static bool dragging = false;
    static Vector2 drag_start_mouse;
    static float drag_start_x_off;
    static float drag_start_y_off;

    // --- INPUT HANDLING ---
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      Vector2 m = GetMousePosition();

      float old_zoom = zoom_factor;
      float new_zoom = zoom_factor * (1.0f + wheel * 0.1f);

      // Clamp zoom
      if (new_zoom < 0.1f)
        new_zoom = 0.1f;
      if (new_zoom > 10.0f)
        new_zoom = 10.0f;

      // World-space point under mouse BEFORE zoom
      float world_x = (m.x - plane_x_off) / old_zoom;
      float world_y = (m.y - plane_y_off) / old_zoom;

      zoom_factor = new_zoom;

      // Convert world point back to screen space AFTER zoom
      plane_x_off = m.x - world_x * new_zoom;
      plane_y_off = m.y - world_y * new_zoom;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      dragging = true;
      select_container();
      drag_start_mouse = GetMousePosition();
      drag_start_x_off = plane_x_off;
      drag_start_y_off = plane_y_off;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      dragging = false;
    }
    if (dragging) {
      Vector2 m = GetMousePosition();
      float dx = m.x - drag_start_mouse.x;
      float dy = m.y - drag_start_mouse.y;
      plane_x_off = drag_start_x_off + dx;
      plane_y_off = drag_start_y_off + dy;
    }

    // --- DRAW ---
    BeginDrawing();

    auto sw = GetScreenWidth();
    auto sh = GetScreenHeight();

    root->wanted_bounds = Bounds(0, 0, sw, sh);
    root->real_bounds = root->wanted_bounds;

    root->pre_layout(root, root, root->real_bounds);
    ::layout(root, root, root->real_bounds);

    paint_root(root);

    EndDrawing();
  }

  CloseWindow();

  return 0;
}
