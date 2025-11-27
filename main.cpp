#include "events.h"
#include "raylib.h"
#include <cmath>
#include <event2/event.h>
#include <exception>
#include <fstream>
#include <string>
#include <format>
#include <strings.h>
#include <vector>

#include "container.h"
#include "event.h"
#include "json.hpp"

#define dpi 1.6f
#define paint [](Container * root, Container * c)
#define fz std::format

static std::vector<Container *> roots;
static std::string clicked_uuid = "";
static int total_steps = 0;
static int current_step = 0;
static float zoom_factor = 1.0;
static float plane_x_off = 0.0;
static float plane_y_off = 0.0;

#define BTN_MOUSE		0x110
#define BTN_LEFT		0x110
#define BTN_RIGHT		0x111
#define BTN_MIDDLE		0x112
#define BTN_SIDE		0x113
#define BTN_EXTRA		0x114
#define BTN_FORWARD		0x115
#define BTN_BACK		0x116
#define BTN_TASK		0x117

// Load a TTF font (TrueType) at a specific size
Font myFont;

void add_line(Container *root, Container *c, int depth) {
    auto line = root->child(FILL_SPACE, 30 * dpi);
    line->skip_delete = true;
    line->user_data = c;
    line->custom_type = depth;
    line->when_paint = paint {
        Container *target = (Container *) c->user_data;
        Rectangle r = {(float) c->real_bounds.x, (float) c->real_bounds.y, (float) c->real_bounds.w, (float) c->real_bounds.h};
        if (clicked_uuid == target->uuid) {
            DrawRectangleRec(r, GREEN);
        } else {
            DrawRectangleRec(r, LIGHTGRAY);
        }
        DrawRectangleLinesEx(r, std::round(1 * dpi), DARKGRAY);
        auto text = "Container: " + ((Container *) c->user_data)->uuid;
        Vector2 pos;
        pos.x = c->real_bounds.x + 10 * dpi + (c->custom_type * (10 * dpi));
        pos.y = c->real_bounds.y + c->real_bounds.h * .5 - (dpi * 18 * .5);
        auto texcolor = BLACK;
        if (!target->exists)
            texcolor = GRAY;
        DrawTextEx(myFont, text.c_str(),
                 pos,
                 dpi * 18, 2.0, texcolor);
    };
    line->when_clicked = paint {
        clicked_uuid = ((Container *) c->user_data)->uuid;
    };
    for (auto child : c->children) {
        add_line(root, child, depth + 1);
    }
};

struct DataLine : UserData {
    std::string text;
};

void add_data_line(Container *root, int depth, std::string text) {
    auto line = root->child(FILL_SPACE, 25 * dpi);
    auto line_data = new DataLine;
    line_data->text = text;
    line->user_data = line_data;
    line->custom_type = depth;
    line->when_paint = paint {
        Rectangle r = {(float) c->real_bounds.x, (float) c->real_bounds.y, (float) c->real_bounds.w, (float) c->real_bounds.h};
        auto line_data = (DataLine *) c->user_data;
        Vector2 pos;
        pos.x = c->real_bounds.x + 10 * dpi + (c->custom_type * (10 * dpi));
        pos.y = c->real_bounds.y + c->real_bounds.h * .5 - (dpi * 18 * .5);
        auto texcolor = BLACK;
        DrawTextEx(myFont, line_data->text.c_str(),
                 pos,
                 dpi * 18, 2.0, texcolor);
    };
};

Container *import_container(const nlohmann::json &j) {
  auto *c = new Container();

  c->uuid = j.value("id", "");
  c->name = c->uuid;

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

  if (c->uuid == clicked_uuid) {
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
      clicked_uuid = p[0]->uuid;
  } else {
      clicked_uuid = "";
  }
}

int main() {
  const int screenWidth = 1400;
  const int screenHeight = 1200;

  // SetConfigFlags(EVENT_BASE_FLAG_IGNORE_ENV);

  InitWindow(screenWidth, screenHeight, "Container debugger");
  myFont = LoadFontEx("/home/jmanc3/.fonts/Roboto-Medium.ttf", 32, 0, 0);
  SetTextureFilter(myFont.texture, TEXTURE_FILTER_BILINEAR); // optional for smoother text
  //SetFont(myFont); // now DrawText() uses this font by default
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
  Container *top_left = nullptr;
  if (auto left = root->child(::vbox, FILL_SPACE, FILL_SPACE)) {
    left->pre_layout = [](Container *root, Container *c, const Bounds &b) {
      c->children[1]->wanted_bounds.h = b.h - (b.h * bottomSplit);
      if (c->children[1]->wanted_bounds.h < 50)
        c->children[1]->wanted_bounds.h = 50;
    };
    auto top = left->child(FILL_SPACE, FILL_SPACE);
    top_left = top;
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
  Container *right_top = nullptr;
  Container *right_bottom = nullptr;
  if (auto right = root->child(400, FILL_SPACE)) {
    right->pre_layout = [](Container *root, Container *c, const Bounds &b) {
      c->children[1]->wanted_bounds.h = b.h - (b.h * rightBottomSplit);
    };

    auto top = right->child(::vbox, FILL_SPACE, FILL_SPACE);
    right_top = top;
    top->type = ::absolute;
    top->pre_layout = [](Container *root, Container *c, const Bounds &b) {
      static int previous_step = -1;
      if (previous_step != current_step) {
        previous_step = current_step;
        for (auto c : c->children)
            delete c;
        c->children.clear();

        add_line(c, roots[current_step], 0);
      }
      auto pre = c->scroll_v_real;
      c->type = ::vbox;
      ::layout(root, c, b);
      c->type = ::absolute;
      c->scroll_v_real = pre;
      for (auto ch : c->children) {
          modify_all(ch, 0, c->scroll_v_real);
      }
    };
    top->when_paint = paint {
      BeginScissorMode(c->real_bounds.x, c->real_bounds.y, c->real_bounds.w,
                       c->real_bounds.h);
      DrawRectangle(c->real_bounds.x, c->real_bounds.y, c->real_bounds.w,
                    c->real_bounds.h, LIGHTGRAY);
    };
    top->after_paint = paint {
        EndScissorMode();  
    };
    auto bottom = right->child(FILL_SPACE, 500);
    right_bottom = bottom;
    bottom->when_paint = paint {
      BeginScissorMode(c->real_bounds.x, c->real_bounds.y, c->real_bounds.w,
                       c->real_bounds.h);
      DrawRectangle(c->real_bounds.x, c->real_bounds.y, c->real_bounds.w,
                    c->real_bounds.h, DARKBLUE);
    };
    bottom->after_paint = paint {
        EndScissorMode();  
    };
    bottom->type = ::absolute;
    bottom->pre_layout = [](Container *root, Container *c, const Bounds &b) {
      static std::string previous_focus = "";
      static int previous_step = -1;
      bool forced = false;
      if (previous_step != current_step) {
        previous_step = current_step;
        forced = true;
      }
      if (previous_focus != clicked_uuid || forced) {
        previous_focus = clicked_uuid;
        
        //assert(false && "Add the data line by line");
        Container *b = container_by_name(clicked_uuid, roots[current_step]);
        if (!b)
            return;
        for (auto c : c->children)
            delete c;
        c->children.clear();

        add_data_line(c, 0, b->uuid);
        add_data_line(c, 0, fz("x: {}", b->real_bounds.x));
        add_data_line(c, 0, fz("y: {}", b->real_bounds.y));
        add_data_line(c, 0, fz("w: {}", b->real_bounds.w));
        add_data_line(c, 0, fz("h: {}", b->real_bounds.h));
        add_data_line(c, 0, fz("hovering: {}", b->state.mouse_hovering));
        add_data_line(c, 0, fz("pressing: {}", b->state.mouse_pressing));
        add_data_line(c, 0, fz("mouse_button_pressed: {}", b->state.mouse_button_pressed));
        add_data_line(c, 0, fz("dragging: {}", b->state.mouse_dragging));
      }
      auto pre = c->scroll_v_real;
      c->type = ::vbox;
      ::layout(root, c, b);
      c->type = ::absolute;
      c->scroll_v_real = pre;
      for (auto ch : c->children) {
          modify_all(ch, 0, c->scroll_v_real);
      }
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

    if (IsKeyPressed(KEY_UP)) {
      current_step++;
      if (current_step > roots.size() - 1)
        current_step = roots.size() - 1;
    }
    if (IsKeyPressed(KEY_DOWN)) {
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
    Vector2 m = GetMousePosition();

    bool mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool mouseReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    if (mousePressed || mouseReleased) {
        Event ev { m.x, m.y, BTN_LEFT, mousePressed ? 1 : 0};
        mouse_event(root, ev);
    }

    if (wheel != 0.0f && bounds_contains(right_top->real_bounds, m.x, m.y)) {
        right_top->scroll_v_visual += wheel * 100;
        right_top->scroll_v_real += wheel * 100;
        if (right_top->scroll_v_real > 0)
           right_top->scroll_v_real = 0;
    }
    if (wheel != 0.0f && bounds_contains(right_bottom->real_bounds, m.x, m.y)) {
        right_bottom->scroll_v_visual += wheel * 100;
        right_bottom->scroll_v_real += wheel * 100;
        if (right_bottom->scroll_v_real > 0)
           right_bottom->scroll_v_real = 0;
     }
    if (wheel != 0.0f && bounds_contains(top_left->real_bounds, m.x, m.y)) {
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

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && bounds_contains(top_left->real_bounds, m.x, m.y)) {
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
