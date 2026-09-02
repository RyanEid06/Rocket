#include "rocket_raylib_adapter.h"

#include <algorithm>

#include <raylib.h>

#include <cmath>
#include <cstdint>
#include <cfloat>
#include <initializer_list>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

struct TextureRecord {
  Texture2D value{};
  int64_t width = 0;
  int64_t height = 0;
  bool native = false;
};

struct SoundRecord {
  Sound value{};
  bool native = false;
};

struct FontRecord {
  Font value{};
  bool native = false;
};

struct AdapterState {
  bool testMode = false;
  bool windowOpen = false;
  bool drawing = false;
  bool audioOpen = false;
  bool closeRequested = false;
  int64_t windowId = 0;
  int64_t frameId = 0;
  int64_t audioId = 0;
  int64_t nextId = 1;
  int64_t drawCount = 0;
  int64_t geometryCallCount = 0;
  int64_t mouseX = 0;
  int64_t mouseY = 0;
  bool mousePressed = false;
  double testTime = 0.0;
  std::unordered_map<int64_t, std::string> buffers;
  std::unordered_map<int64_t, std::vector<Vector2>> pointBuffers;
  std::unordered_map<int64_t, TextureRecord> textures;
  std::unordered_map<int64_t, FontRecord> fonts;
  std::unordered_map<int64_t, SoundRecord> sounds;
  std::unordered_set<int64_t> pressedKeys;
  std::unordered_set<int64_t> downKeys;
};

AdapterState state;

bool fitsInt(int64_t value) {
  return value >= std::numeric_limits<int>::min() &&
         value <= std::numeric_limits<int>::max();
}

bool byteComponent(int64_t value) { return value >= 0 && value <= 255; }

bool validColor(int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  return byteComponent(red) && byteComponent(green) && byteComponent(blue) &&
         byteComponent(alpha);
}

bool finiteFloat(double value) {
  return std::isfinite(value) && std::abs(value) <= FLT_MAX;
}

bool finiteFloats(std::initializer_list<double> values) {
  for (double value : values) if (!finiteFloat(value)) return false;
  return true;
}

Rectangle rectangle(double x, double y, double width, double height) {
  return Rectangle{static_cast<float>(x), static_cast<float>(y),
                   static_cast<float>(width), static_cast<float>(height)};
}

Vector2 point(double x, double y) {
  return Vector2{static_cast<float>(x), static_cast<float>(y)};
}

int64_t requireDrawing(int64_t frameId);

int64_t beginGeometry(int64_t frameId) {
  ++state.geometryCallCount;
  return requireDrawing(frameId);
}

Color color(int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  return Color{static_cast<unsigned char>(red), static_cast<unsigned char>(green),
               static_cast<unsigned char>(blue), static_cast<unsigned char>(alpha)};
}

int64_t nextId() {
  if (state.nextId == std::numeric_limits<int64_t>::max()) state.nextId = 1;
  return state.nextId++;
}

bool validWindow(int64_t id) { return state.windowOpen && id == state.windowId; }

bool validAudio(int64_t id) { return state.audioOpen && id == state.audioId; }

const std::string* buffer(int64_t id) {
  const auto found = state.buffers.find(id);
  return found == state.buffers.end() ? nullptr : &found->second;
}

bool simulatedMissing(const std::string& path) {
  return path.empty() || path.find("missing") != std::string::npos;
}

int64_t requireDrawing(int64_t frameId) {
  return state.windowOpen && state.drawing && frameId == state.frameId
             ? RLV_OK
             : RLV_ERR_STALE_HANDLE;
}

}  // namespace

extern "C" int64_t rlv_version_major(void) { return RAYLIB_VERSION_MAJOR; }

extern "C" int64_t rlv_version_minor(void) { return RAYLIB_VERSION_MINOR; }

extern "C" int64_t rlv_enable_test_mode(rocket_bool enabled) {
  if (state.windowOpen || state.audioOpen || !state.textures.empty() ||
      !state.fonts.empty() || !state.sounds.empty() || !state.pointBuffers.empty()) {
    return RLV_ERR_RESOURCE_LIVE;
  }
  state.testMode = enabled != 0;
  return RLV_OK;
}

extern "C" int64_t rlv_test_reset(void) {
  if (!state.testMode) return RLV_ERR_INVALID_STATE;
  state = AdapterState{};
  state.testMode = true;
  return RLV_OK;
}

extern "C" int64_t rlv_buffer_create(void) {
  const int64_t id = nextId();
  state.buffers.emplace(id, std::string{});
  return id;
}

extern "C" int64_t rlv_buffer_push(int64_t bufferId, uint8_t byteValue) {
  const auto found = state.buffers.find(bufferId);
  if (found == state.buffers.end()) return RLV_ERR_STALE_HANDLE;
  if (byteValue == 0) return RLV_ERR_INVALID_ARGUMENT;
  found->second.push_back(static_cast<char>(byteValue));
  return RLV_OK;
}

extern "C" int64_t rlv_buffer_destroy(int64_t bufferId) {
  return state.buffers.erase(bufferId) == 1 ? RLV_OK : RLV_ERR_STALE_HANDLE;
}

extern "C" int64_t rlv_buffer_live_count(void) {
  return static_cast<int64_t>(state.buffers.size());
}

extern "C" int64_t rlv_point_buffer_create(void) {
  const int64_t id = nextId();
  state.pointBuffers.emplace(id, std::vector<Vector2>{});
  return id;
}

extern "C" int64_t rlv_point_buffer_push(int64_t bufferId, double x, double y) {
  const auto found = state.pointBuffers.find(bufferId);
  if (found == state.pointBuffers.end()) return RLV_ERR_STALE_HANDLE;
  if (!finiteFloats({x, y})) return RLV_ERR_INVALID_ARGUMENT;
  found->second.push_back(point(x, y));
  return RLV_OK;
}

extern "C" int64_t rlv_point_buffer_destroy(int64_t bufferId) {
  return state.pointBuffers.erase(bufferId) == 1 ? RLV_OK : RLV_ERR_STALE_HANDLE;
}

extern "C" int64_t rlv_point_buffer_live_count(void) {
  return static_cast<int64_t>(state.pointBuffers.size());
}

extern "C" int64_t rlv_window_open(int64_t width, int64_t height,
                                     int64_t titleBufferId) {
  const std::string* title = buffer(titleBufferId);
  if (!title || width <= 0 || height <= 0 || !fitsInt(width) || !fitsInt(height)) {
    return RLV_ERR_INVALID_ARGUMENT;
  }
  if (state.windowOpen) return RLV_ERR_INVALID_STATE;
  if (!state.testMode) {
    InitWindow(static_cast<int>(width), static_cast<int>(height), title->c_str());
    if (!IsWindowReady()) return RLV_ERR_UNAVAILABLE;
  }
  state.windowOpen = true;
  state.closeRequested = false;
  state.windowId = nextId();
  return state.windowId;
}

extern "C" int64_t rlv_window_close(int64_t windowId) {
  if (!validWindow(windowId)) return RLV_ERR_STALE_HANDLE;
  if (state.drawing) return RLV_ERR_INVALID_STATE;
  if (!state.textures.empty() || !state.fonts.empty()) return RLV_ERR_RESOURCE_LIVE;
  if (!state.testMode) CloseWindow();
  state.windowOpen = false;
  state.windowId = 0;
  state.closeRequested = false;
  return RLV_OK;
}

extern "C" rocket_bool rlv_window_ready(int64_t windowId) {
  if (!validWindow(windowId)) return 0;
  return state.testMode || IsWindowReady() ? 1 : 0;
}

extern "C" rocket_bool rlv_window_should_close(int64_t windowId) {
  if (!validWindow(windowId)) return 1;
  return state.testMode ? static_cast<rocket_bool>(state.closeRequested)
                        : static_cast<rocket_bool>(WindowShouldClose());
}

extern "C" int64_t rlv_set_target_fps(int64_t windowId, int64_t fps) {
  if (!validWindow(windowId)) return RLV_ERR_STALE_HANDLE;
  if (fps <= 0 || !fitsInt(fps)) return RLV_ERR_INVALID_ARGUMENT;
  if (!state.testMode) SetTargetFPS(static_cast<int>(fps));
  return RLV_OK;
}

extern "C" double rlv_frame_time(int64_t windowId) {
  if (!validWindow(windowId)) return 0.0;
  return state.testMode ? 1.0 / 60.0 : static_cast<double>(GetFrameTime());
}

extern "C" double rlv_time(int64_t windowId) {
  if (!validWindow(windowId)) return 0.0;
  return state.testMode ? state.testTime : GetTime();
}

extern "C" int64_t rlv_begin_drawing(int64_t windowId) {
  if (!validWindow(windowId)) return RLV_ERR_STALE_HANDLE;
  if (state.drawing) return RLV_ERR_INVALID_STATE;
  if (!state.testMode) BeginDrawing();
  state.drawing = true;
  state.frameId = nextId();
  return state.frameId;
}

extern "C" int64_t rlv_end_drawing(int64_t frameId) {
  if (requireDrawing(frameId) != RLV_OK) return RLV_ERR_STALE_HANDLE;
  if (!state.testMode) EndDrawing();
  state.drawing = false;
  state.frameId = 0;
  state.testTime += 1.0 / 60.0;
  return RLV_OK;
}

extern "C" int64_t rlv_clear_background(int64_t frameId, int64_t red, int64_t green,
                                          int64_t blue, int64_t alpha) {
  if (requireDrawing(frameId) != RLV_OK) return RLV_ERR_STALE_HANDLE;
  if (!validColor(red, green, blue, alpha)) return RLV_ERR_INVALID_ARGUMENT;
  if (!state.testMode) ClearBackground(color(red, green, blue, alpha));
  ++state.drawCount;
  return RLV_OK;
}

extern "C" int64_t rlv_draw_rectangle(int64_t frameId, int64_t x, int64_t y, int64_t width,
                                        int64_t height, int64_t red, int64_t green,
                                        int64_t blue, int64_t alpha) {
  if (beginGeometry(frameId) != RLV_OK) return RLV_ERR_STALE_HANDLE;
  if (!fitsInt(x) || !fitsInt(y) || !fitsInt(width) || !fitsInt(height) ||
      width < 0 || height < 0 || !validColor(red, green, blue, alpha)) {
    return RLV_ERR_INVALID_ARGUMENT;
  }
  if (!state.testMode) {
    DrawRectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width),
                  static_cast<int>(height), color(red, green, blue, alpha));
  }
  ++state.drawCount;
  return RLV_OK;
}

extern "C" int64_t rlv_draw_circle(int64_t frameId, int64_t x, int64_t y, double radius,
                                     int64_t red, int64_t green, int64_t blue,
                                     int64_t alpha) {
  if (beginGeometry(frameId) != RLV_OK) return RLV_ERR_STALE_HANDLE;
  if (!fitsInt(x) || !fitsInt(y) || radius < 0.0 ||
      !finiteFloat(radius) || !validColor(red, green, blue, alpha)) {
    return RLV_ERR_INVALID_ARGUMENT;
  }
  if (!state.testMode) {
    DrawCircle(static_cast<int>(x), static_cast<int>(y), static_cast<float>(radius),
               color(red, green, blue, alpha));
  }
  ++state.drawCount;
  return RLV_OK;
}

#define RLV_GEOMETRY_GUARD(frame_id, condition) \
  do { \
    if (beginGeometry(frame_id) != RLV_OK) return RLV_ERR_STALE_HANDLE; \
    if (!(condition)) return RLV_ERR_INVALID_ARGUMENT; \
  } while (false)

#define RLV_DRAW_DONE() \
  do { ++state.drawCount; return RLV_OK; } while (false)

extern "C" int64_t rlv_draw_rectangle_outline(int64_t frameId, double x, double y,
    double width, double height, double thickness, int64_t red, int64_t green,
    int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x, y, width, height, thickness}) &&
      width >= 0.0 && height >= 0.0 && thickness >= 0.0 && validColor(red, green, blue, alpha));
  if (!state.testMode) DrawRectangleLinesEx(rectangle(x, y, width, height), static_cast<float>(thickness), color(red, green, blue, alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_rounded_rectangle(int64_t frameId, double x, double y,
    double width, double height, double roundness, int64_t red, int64_t green,
    int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x, y, width, height, roundness}) && width >= 0.0 &&
      height >= 0.0 && roundness >= 0.0 && roundness <= 1.0 && validColor(red, green, blue, alpha));
  if (!state.testMode) DrawRectangleRounded(rectangle(x, y, width, height), static_cast<float>(roundness), 0, color(red, green, blue, alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_rounded_rectangle_outline(int64_t frameId, double x, double y,
    double width, double height, double roundness, double thickness, int64_t red,
    int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x, y, width, height, roundness, thickness}) &&
      width >= 0.0 && height >= 0.0 && roundness >= 0.0 && roundness <= 1.0 &&
      thickness >= 0.0 && validColor(red, green, blue, alpha));
  if (!state.testMode) DrawRectangleRoundedLinesEx(rectangle(x, y, width, height), static_cast<float>(roundness), 0, static_cast<float>(thickness), color(red, green, blue, alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_rectangle_gradient_vertical(int64_t frameId, double x, double y,
    double width, double height, int64_t tr, int64_t tg, int64_t tb, int64_t ta,
    int64_t br, int64_t bg, int64_t bb, int64_t ba) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x, y, width, height}) && width >= 0.0 && height >= 0.0 &&
      validColor(tr, tg, tb, ta) && validColor(br, bg, bb, ba));
  if (!state.testMode) DrawRectangleGradientEx(rectangle(x, y, width, height), color(tr,tg,tb,ta), color(br,bg,bb,ba), color(br,bg,bb,ba), color(tr,tg,tb,ta));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_rectangle_gradient_horizontal(int64_t frameId, double x, double y,
    double width, double height, int64_t lr, int64_t lg, int64_t lb, int64_t la,
    int64_t rr, int64_t rg, int64_t rb, int64_t ra) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x, y, width, height}) && width >= 0.0 && height >= 0.0 &&
      validColor(lr, lg, lb, la) && validColor(rr, rg, rb, ra));
  if (!state.testMode) DrawRectangleGradientEx(rectangle(x, y, width, height), color(lr,lg,lb,la), color(lr,lg,lb,la), color(rr,rg,rb,ra), color(rr,rg,rb,ra));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_rectangle_gradient_four(int64_t frameId, double x, double y,
    double width, double height, int64_t tlr, int64_t tlg, int64_t tlb, int64_t tla,
    int64_t blr, int64_t blg, int64_t blb, int64_t bla, int64_t brr, int64_t brg,
    int64_t brb, int64_t bra, int64_t trr, int64_t trg, int64_t trb, int64_t tra) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x, y, width, height}) && width >= 0.0 && height >= 0.0 &&
      validColor(tlr,tlg,tlb,tla) && validColor(blr,blg,blb,bla) &&
      validColor(brr,brg,brb,bra) && validColor(trr,trg,trb,tra));
  if (!state.testMode) DrawRectangleGradientEx(rectangle(x,y,width,height), color(tlr,tlg,tlb,tla), color(blr,blg,blb,bla), color(brr,brg,brb,bra), color(trr,trg,trb,tra));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_circle_outline(int64_t frameId, double x, double y, double radius,
    double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x,y,radius,thickness}) && radius >= 0.0 && thickness >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawRing(point(x,y), static_cast<float>(std::max(0.0, radius - thickness)), static_cast<float>(radius), 0.0f, 360.0f, 0, color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_ellipse(int64_t frameId, double x, double y, double rx, double ry,
    int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x,y,rx,ry}) && rx >= 0.0 && ry >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawEllipseV(point(x,y), static_cast<float>(rx), static_cast<float>(ry), color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_ellipse_outline(int64_t frameId, double x, double y, double rx, double ry,
    int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x,y,rx,ry}) && rx >= 0.0 && ry >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawEllipseLinesV(point(x,y), static_cast<float>(rx), static_cast<float>(ry), color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

static bool validRing(double x, double y, double inner, double outer) {
  return finiteFloats({x,y,inner,outer}) && inner >= 0.0 && outer >= inner;
}

extern "C" int64_t rlv_draw_ring(int64_t frameId, double x, double y, double inner, double outer,
    int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, validRing(x,y,inner,outer) && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawRing(point(x,y), static_cast<float>(inner), static_cast<float>(outer), 0.0f, 360.0f, 0, color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_ring_outline(int64_t frameId, double x, double y, double inner, double outer,
    int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, validRing(x,y,inner,outer) && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawRingLines(point(x,y), static_cast<float>(inner), static_cast<float>(outer), 0.0f, 360.0f, 0, color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_ring_sector(int64_t frameId, double x, double y, double inner, double outer,
    double start, double end, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, validRing(x,y,inner,outer) && finiteFloats({start,end}) && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawRing(point(x,y), static_cast<float>(inner), static_cast<float>(outer), static_cast<float>(start), static_cast<float>(end), 0, color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_circle_sector(int64_t frameId, double x, double y, double radius,
    double start, double end, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x,y,radius,start,end}) && radius >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawCircleSector(point(x,y), static_cast<float>(radius), static_cast<float>(start), static_cast<float>(end), 0, color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_circle_sector_outline(int64_t frameId, double x, double y, double radius,
    double start, double end, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x,y,radius,start,end}) && radius >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawCircleSectorLines(point(x,y), static_cast<float>(radius), static_cast<float>(start), static_cast<float>(end), 0, color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_circle_gradient(int64_t frameId, double x, double y, double radius,
    int64_t ir, int64_t ig, int64_t ib, int64_t ia, int64_t or_, int64_t og, int64_t ob, int64_t oa) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x,y,radius}) && radius >= 0.0 && validColor(ir,ig,ib,ia) && validColor(or_,og,ob,oa));
  if (!state.testMode) DrawCircleGradient(point(x,y), static_cast<float>(radius), color(ir,ig,ib,ia), color(or_,og,ob,oa));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_line(int64_t frameId, double sx, double sy, double ex, double ey,
    int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({sx,sy,ex,ey}) && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawLineV(point(sx,sy), point(ex,ey), color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_thick_line(int64_t frameId, double sx, double sy, double ex, double ey,
    double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({sx,sy,ex,ey,thickness}) && thickness >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawLineEx(point(sx,sy), point(ex,ey), static_cast<float>(thickness), color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_triangle(int64_t frameId, double ax, double ay, double bx, double by,
    double cx, double cy, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({ax,ay,bx,by,cx,cy}) && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawTriangle(point(ax,ay), point(bx,by), point(cx,cy), color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_triangle_outline(int64_t frameId, double ax, double ay, double bx, double by,
    double cx, double cy, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({ax,ay,bx,by,cx,cy,thickness}) && thickness >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) { const Color value=color(red,green,blue,alpha); DrawLineEx(point(ax,ay),point(bx,by),static_cast<float>(thickness),value); DrawLineEx(point(bx,by),point(cx,cy),static_cast<float>(thickness),value); DrawLineEx(point(cx,cy),point(ax,ay),static_cast<float>(thickness),value); }
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_polygon(int64_t frameId, double x, double y, int64_t sides,
    double radius, double rotation, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x,y,radius,rotation}) && fitsInt(sides) && sides >= 3 && radius >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawPoly(point(x,y), static_cast<int>(sides), static_cast<float>(radius), static_cast<float>(rotation), color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_polygon_outline(int64_t frameId, double x, double y, int64_t sides,
    double radius, double rotation, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({x,y,radius,rotation,thickness}) && fitsInt(sides) && sides >= 3 && radius >= 0.0 && thickness >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawPolyLinesEx(point(x,y), static_cast<int>(sides), static_cast<float>(radius), static_cast<float>(rotation), static_cast<float>(thickness), color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

extern "C" int64_t rlv_draw_bezier_line(int64_t frameId, double sx, double sy, double ex, double ey,
    double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  RLV_GEOMETRY_GUARD(frameId, finiteFloats({sx,sy,ex,ey,thickness}) && thickness >= 0.0 && validColor(red,green,blue,alpha));
  if (!state.testMode) DrawLineBezier(point(sx,sy), point(ex,ey), static_cast<float>(thickness), color(red,green,blue,alpha));
  RLV_DRAW_DONE();
}

static int64_t drawBezier(int64_t frameId, int64_t bufferId, double thickness,
    int64_t red, int64_t green, int64_t blue, int64_t alpha, bool cubic) {
  if (beginGeometry(frameId) != RLV_OK) return RLV_ERR_STALE_HANDLE;
  const auto found = state.pointBuffers.find(bufferId);
  if (found == state.pointBuffers.end()) return RLV_ERR_STALE_HANDLE;
  const std::size_t count = found->second.size();
  const bool validCount = cubic ? count >= 4 && (count - 1) % 3 == 0
                                : count >= 3 && (count - 1) % 2 == 0;
  if (!validCount || count > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
      !finiteFloat(thickness) || thickness < 0.0 || !validColor(red,green,blue,alpha)) return RLV_ERR_INVALID_ARGUMENT;
  if (!state.testMode) {
    if (cubic) DrawSplineBezierCubic(found->second.data(), static_cast<int>(count), static_cast<float>(thickness), color(red,green,blue,alpha));
    else DrawSplineBezierQuadratic(found->second.data(), static_cast<int>(count), static_cast<float>(thickness), color(red,green,blue,alpha));
  }
  ++state.drawCount;
  return RLV_OK;
}

extern "C" int64_t rlv_draw_bezier_quadratic(int64_t frameId, int64_t bufferId, double thickness,
    int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  return drawBezier(frameId, bufferId, thickness, red, green, blue, alpha, false);
}

extern "C" int64_t rlv_draw_bezier_cubic(int64_t frameId, int64_t bufferId, double thickness,
    int64_t red, int64_t green, int64_t blue, int64_t alpha) {
  return drawBezier(frameId, bufferId, thickness, red, green, blue, alpha, true);
}

extern "C" int64_t rlv_geometry_call_count(void) { return state.geometryCallCount; }

#undef RLV_DRAW_DONE
#undef RLV_GEOMETRY_GUARD

extern "C" int64_t rlv_draw_text(int64_t frameId, int64_t textBufferId, int64_t x, int64_t y,
                                   int64_t size, int64_t red, int64_t green,
                                   int64_t blue, int64_t alpha) {
  const std::string* text = buffer(textBufferId);
  if (requireDrawing(frameId) != RLV_OK) return RLV_ERR_STALE_HANDLE;
  if (!text || !fitsInt(x) || !fitsInt(y) || size <= 0 || !fitsInt(size) ||
      !validColor(red, green, blue, alpha)) {
    return RLV_ERR_INVALID_ARGUMENT;
  }
  if (!state.testMode) {
    DrawText(text->c_str(), static_cast<int>(x), static_cast<int>(y),
             static_cast<int>(size), color(red, green, blue, alpha));
  }
  ++state.drawCount;
  return RLV_OK;
}

extern "C" int64_t rlv_draw_count(void) { return state.drawCount; }

extern "C" rocket_bool rlv_key_pressed(int64_t windowId, int64_t key) {
  if (!validWindow(windowId) || !fitsInt(key)) return 0;
  if (!state.testMode) return IsKeyPressed(static_cast<int>(key)) ? 1 : 0;
  const auto found = state.pressedKeys.find(key);
  if (found == state.pressedKeys.end()) return 0;
  state.pressedKeys.erase(found);
  return 1;
}

extern "C" rocket_bool rlv_key_down(int64_t windowId, int64_t key) {
  if (!validWindow(windowId) || !fitsInt(key)) return 0;
  return state.testMode
             ? static_cast<rocket_bool>(state.downKeys.find(key) != state.downKeys.end())
             : static_cast<rocket_bool>(IsKeyDown(static_cast<int>(key)));
}

extern "C" rocket_bool rlv_mouse_pressed(int64_t windowId, int64_t button) {
  if (!validWindow(windowId) || !fitsInt(button)) return 0;
  if (!state.testMode) return IsMouseButtonPressed(static_cast<int>(button)) ? 1 : 0;
  const bool pressed = state.mousePressed;
  state.mousePressed = false;
  return pressed ? 1 : 0;
}

extern "C" int64_t rlv_mouse_x(int64_t windowId) {
  if (!validWindow(windowId)) return 0;
  return state.testMode ? state.mouseX : GetMouseX();
}

extern "C" int64_t rlv_mouse_y(int64_t windowId) {
  if (!validWindow(windowId)) return 0;
  return state.testMode ? state.mouseY : GetMouseY();
}

extern "C" int64_t rlv_texture_load(int64_t windowId, int64_t pathBufferId) {
  const std::string* path = buffer(pathBufferId);
  if (!validWindow(windowId)) return RLV_ERR_STALE_HANDLE;
  if (!path) return RLV_ERR_INVALID_ARGUMENT;
  TextureRecord record;
  if (state.testMode) {
    if (simulatedMissing(*path)) return RLV_ERR_NOT_FOUND;
    record.width = 64;
    record.height = 64;
  } else {
    record.value = LoadTexture(path->c_str());
    if (!IsTextureValid(record.value)) return RLV_ERR_NOT_FOUND;
    record.width = record.value.width;
    record.height = record.value.height;
    record.native = true;
  }
  const int64_t id = nextId();
  state.textures.emplace(id, record);
  return id;
}

extern "C" int64_t rlv_texture_width(int64_t textureId) {
  const auto found = state.textures.find(textureId);
  return found == state.textures.end() ? RLV_ERR_STALE_HANDLE : found->second.width;
}

extern "C" int64_t rlv_texture_height(int64_t textureId) {
  const auto found = state.textures.find(textureId);
  return found == state.textures.end() ? RLV_ERR_STALE_HANDLE : found->second.height;
}

extern "C" int64_t rlv_texture_draw(int64_t frameId, int64_t textureId, int64_t x, int64_t y,
                                      int64_t red, int64_t green, int64_t blue,
                                      int64_t alpha) {
  const auto found = state.textures.find(textureId);
  if (requireDrawing(frameId) != RLV_OK) return RLV_ERR_STALE_HANDLE;
  if (found == state.textures.end()) return RLV_ERR_STALE_HANDLE;
  if (!fitsInt(x) || !fitsInt(y) || !validColor(red, green, blue, alpha)) {
    return RLV_ERR_INVALID_ARGUMENT;
  }
  if (!state.testMode) {
    DrawTexture(found->second.value, static_cast<int>(x), static_cast<int>(y),
                color(red, green, blue, alpha));
  }
  ++state.drawCount;
  return RLV_OK;
}

extern "C" int64_t rlv_texture_draw_scaled(int64_t frameId, int64_t textureId,
                                             int64_t x, int64_t y, double scale,
                                             int64_t red, int64_t green,
                                             int64_t blue, int64_t alpha) {
  const auto found = state.textures.find(textureId);
  if (requireDrawing(frameId) != RLV_OK) return RLV_ERR_STALE_HANDLE;
  if (found == state.textures.end()) return RLV_ERR_STALE_HANDLE;
  if (!fitsInt(x) || !fitsInt(y) || !std::isfinite(scale) || scale <= 0.0 ||
      !validColor(red, green, blue, alpha)) {
    return RLV_ERR_INVALID_ARGUMENT;
  }
  if (!state.testMode) {
    DrawTextureEx(found->second.value,
                  Vector2{static_cast<float>(x), static_cast<float>(y)}, 0.0f,
                  static_cast<float>(scale), color(red, green, blue, alpha));
  }
  ++state.drawCount;
  return RLV_OK;
}

extern "C" int64_t rlv_texture_unload(int64_t textureId) {
  const auto found = state.textures.find(textureId);
  if (found == state.textures.end()) return RLV_ERR_STALE_HANDLE;
  if (state.drawing) return RLV_ERR_INVALID_STATE;
  if (found->second.native) UnloadTexture(found->second.value);
  state.textures.erase(found);
  return RLV_OK;
}

extern "C" int64_t rlv_texture_live_count(void) {
  return static_cast<int64_t>(state.textures.size());
}

extern "C" int64_t rlv_font_load(int64_t windowId, int64_t pathBufferId) {
  const std::string* path = buffer(pathBufferId);
  if (!validWindow(windowId)) return RLV_ERR_STALE_HANDLE;
  if (!path) return RLV_ERR_INVALID_ARGUMENT;
  FontRecord record;
  if (state.testMode) {
    if (simulatedMissing(*path)) return RLV_ERR_NOT_FOUND;
  } else {
    record.value = LoadFont(path->c_str());
    if (!IsFontValid(record.value)) return RLV_ERR_NOT_FOUND;
    record.native = true;
  }
  const int64_t id = nextId();
  state.fonts.emplace(id, record);
  return id;
}

extern "C" int64_t rlv_font_draw(int64_t frameId, int64_t fontId,
                                   int64_t textBufferId, int64_t x, int64_t y,
                                   double size, double spacing, int64_t red,
                                   int64_t green, int64_t blue, int64_t alpha) {
  const auto found = state.fonts.find(fontId);
  const std::string* text = buffer(textBufferId);
  if (requireDrawing(frameId) != RLV_OK) return RLV_ERR_STALE_HANDLE;
  if (found == state.fonts.end()) return RLV_ERR_STALE_HANDLE;
  if (!text || !fitsInt(x) || !fitsInt(y) || !std::isfinite(size) ||
      !std::isfinite(spacing) || size <= 0.0 || spacing < 0.0 ||
      !validColor(red, green, blue, alpha)) {
    return RLV_ERR_INVALID_ARGUMENT;
  }
  if (!state.testMode) {
    DrawTextEx(found->second.value,
               text->c_str(),
               Vector2{static_cast<float>(x), static_cast<float>(y)},
               static_cast<float>(size), static_cast<float>(spacing),
               color(red, green, blue, alpha));
  }
  ++state.drawCount;
  return RLV_OK;
}

extern "C" int64_t rlv_font_unload(int64_t fontId) {
  const auto found = state.fonts.find(fontId);
  if (found == state.fonts.end()) return RLV_ERR_STALE_HANDLE;
  if (state.drawing) return RLV_ERR_INVALID_STATE;
  if (found->second.native) UnloadFont(found->second.value);
  state.fonts.erase(found);
  return RLV_OK;
}

extern "C" int64_t rlv_font_live_count(void) {
  return static_cast<int64_t>(state.fonts.size());
}

extern "C" int64_t rlv_audio_open(void) {
  if (state.audioOpen) return RLV_ERR_INVALID_STATE;
  if (!state.testMode) {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return RLV_ERR_UNAVAILABLE;
  }
  state.audioOpen = true;
  state.audioId = nextId();
  return state.audioId;
}

extern "C" int64_t rlv_audio_close(int64_t audioId) {
  if (!validAudio(audioId)) return RLV_ERR_STALE_HANDLE;
  if (!state.sounds.empty()) return RLV_ERR_RESOURCE_LIVE;
  if (!state.testMode) CloseAudioDevice();
  state.audioOpen = false;
  state.audioId = 0;
  return RLV_OK;
}

extern "C" rocket_bool rlv_audio_ready(int64_t audioId) {
  if (!validAudio(audioId)) return 0;
  return state.testMode || IsAudioDeviceReady() ? 1 : 0;
}

extern "C" int64_t rlv_sound_load(int64_t audioId, int64_t pathBufferId) {
  const std::string* path = buffer(pathBufferId);
  if (!validAudio(audioId)) return RLV_ERR_STALE_HANDLE;
  if (!path) return RLV_ERR_INVALID_ARGUMENT;
  SoundRecord record;
  if (state.testMode) {
    if (simulatedMissing(*path)) return RLV_ERR_NOT_FOUND;
  } else {
    record.value = LoadSound(path->c_str());
    if (!IsSoundValid(record.value)) return RLV_ERR_NOT_FOUND;
    record.native = true;
  }
  const int64_t id = nextId();
  state.sounds.emplace(id, record);
  return id;
}

extern "C" int64_t rlv_sound_tone(int64_t audioId, double frequency,
                                    double seconds) {
  if (!validAudio(audioId)) return RLV_ERR_STALE_HANDLE;
  if (!std::isfinite(frequency) || !std::isfinite(seconds) || frequency < 20.0 ||
      frequency > 20000.0 || seconds <= 0.0 || seconds > 10.0) {
    return RLV_ERR_INVALID_ARGUMENT;
  }
  SoundRecord record;
  if (!state.testMode) {
    constexpr unsigned int sampleRate = 44100;
    const auto frameCount = static_cast<unsigned int>(sampleRate * seconds);
    std::vector<float> samples(frameCount);
    constexpr double tau = 6.28318530717958647692;
    for (unsigned int index = 0; index < frameCount; ++index) {
      samples[index] = static_cast<float>(0.20 *
          std::sin(tau * frequency * static_cast<double>(index) / sampleRate));
    }
    Wave wave{frameCount, sampleRate, 32, 1, samples.data()};
    record.value = LoadSoundFromWave(wave);
    if (!IsSoundValid(record.value)) return RLV_ERR_UNAVAILABLE;
    record.native = true;
  }
  const int64_t id = nextId();
  state.sounds.emplace(id, record);
  return id;
}

extern "C" int64_t rlv_sound_play(int64_t soundId) {
  const auto found = state.sounds.find(soundId);
  if (found == state.sounds.end()) return RLV_ERR_STALE_HANDLE;
  if (!state.testMode) PlaySound(found->second.value);
  return RLV_OK;
}

extern "C" int64_t rlv_sound_stop(int64_t soundId) {
  const auto found = state.sounds.find(soundId);
  if (found == state.sounds.end()) return RLV_ERR_STALE_HANDLE;
  if (!state.testMode) StopSound(found->second.value);
  return RLV_OK;
}

extern "C" int64_t rlv_sound_set_volume(int64_t soundId, double volume) {
  const auto found = state.sounds.find(soundId);
  if (found == state.sounds.end()) return RLV_ERR_STALE_HANDLE;
  if (!std::isfinite(volume) || volume < 0.0 || volume > 1.0) {
    return RLV_ERR_INVALID_ARGUMENT;
  }
  if (!state.testMode) SetSoundVolume(found->second.value, static_cast<float>(volume));
  return RLV_OK;
}

extern "C" int64_t rlv_sound_unload(int64_t soundId) {
  const auto found = state.sounds.find(soundId);
  if (found == state.sounds.end()) return RLV_ERR_STALE_HANDLE;
  if (found->second.native) UnloadSound(found->second.value);
  state.sounds.erase(found);
  return RLV_OK;
}

extern "C" int64_t rlv_sound_live_count(void) {
  return static_cast<int64_t>(state.sounds.size());
}

extern "C" int64_t rlv_apply_callback(RlvIntCallback callback, int64_t value) {
  return callback ? callback(value) : RLV_ERR_INVALID_ARGUMENT;
}

extern "C" int64_t rlv_test_set_key(int64_t key, rocket_bool pressed,
                                      rocket_bool down) {
  if (!state.testMode || !fitsInt(key)) return RLV_ERR_INVALID_STATE;
  if (pressed) state.pressedKeys.insert(key);
  else state.pressedKeys.erase(key);
  if (down) state.downKeys.insert(key);
  else state.downKeys.erase(key);
  return RLV_OK;
}

extern "C" int64_t rlv_test_set_mouse(int64_t x, int64_t y,
                                        rocket_bool pressed) {
  if (!state.testMode || !fitsInt(x) || !fitsInt(y)) return RLV_ERR_INVALID_STATE;
  state.mouseX = x;
  state.mouseY = y;
  state.mousePressed = pressed != 0;
  return RLV_OK;
}

extern "C" int64_t rlv_test_request_close(rocket_bool requested) {
  if (!state.testMode) return RLV_ERR_INVALID_STATE;
  state.closeRequested = requested != 0;
  return RLV_OK;
}
