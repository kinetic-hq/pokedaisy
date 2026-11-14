/* The right wing's SAVE STATE and CLOSE GAME under a finger, a mouse click and a pen (a mouse to SDL): each lands on
 * its button, whatever logical size, scale or window size the renderer was left with. SDL's renderer moves a finger's
 * x / y into its viewport and clamps a touch outside it to the edge, so a tap on a button under a letterboxed game
 * landed on the wing's edge and opened the companion; main.c's finger watch keeps the window's own x / y. */
#define POCKET_TEST
#define main linux_app_main
#include "main.c"
#undef main
#include <assert.h>

static void test_frame(struct mCore *c, bool companion, int frame, struct pd_ui *ui, struct pd_snapshot *snap) {
  (void)c;
  (void)companion;
  (void)frame;
  (void)ui;
  (void)snap;
}

// The wing as draw_panel leaves it: its canvas (W x H, the buttons at its foot) drawn at `at`.
static void wing_at(SDL_Rect at, int w, int h) {
  rightWingMap = true;
  rightWingAt = at;
  rightWingW = w;
  rightWingH = h;
  memset(pd_ui_wing_buttons, 0, sizeof pd_ui_wing_buttons);
  pd_ui_wing_buttons[PD_WING_SAVE_STATE] = (struct pd_ui_rect){4, h - 40, w - 8, 16};
  pd_ui_wing_buttons[PD_WING_CLOSE_GAME] = (struct pd_ui_rect){4, h - 20, w - 8, 16};
}

// The window pixel at the middle of button B.
static void button_middle(int b, float *x, float *y) {
  const struct pd_ui_rect *r = &pd_ui_wing_buttons[b];
  *x = rightWingAt.x + (r->x + r->w / 2.0f) * rightWingAt.w / rightWingW;
  *y = rightWingAt.y + (r->y + r->h / 2.0f) * rightWingAt.h / rightWingH;
}

// The one click_point() event the queue gives back after pushing E, mapped to the renderer's pixels.
static bool pushed_point(SDL_Event *e, float *x, float *y) {
  assert(SDL_PushEvent(e) == 1);
  SDL_Event got;
  bool found = false;
  while (SDL_PollEvent(&got))
    if (!found && click_point(&got, x, y))
      found = true;
  return found;
}

static int checks;
static void check_buttons(int ww, int wh, const char *setup) {
  for (int b = 0; b < PD_WING_BUTTONS; b++) {
    float bx, by, x, y;
    button_middle(b, &bx, &by);
    // A finger: normalized to the window, as Windows' touch and SDL give it.
    SDL_Event f;
    SDL_zero(f);
    f.type = SDL_FINGERDOWN;
    f.tfinger.timestamp = SDL_GetTicks() + (Uint32)checks;
    f.tfinger.fingerId = 3;
    f.tfinger.windowID = SDL_GetWindowID(window);
    f.tfinger.x = bx / ww;
    f.tfinger.y = by / wh;
    assert(pushed_point(&f, &x, &y));
    if (wing_button_at(x, y) != b) {
      fprintf(stderr, "%dx%d %s: a finger on button %d at %.0f,%.0f came in at %.0f,%.0f\n", ww, wh, setup, b, bx, by,
              x, y);
      assert(0);
    }
    // A mouse or a pen: window points, which SDL's renderer turns to its logical size (mouse_event_at turns back).
    SDL_Event m;
    SDL_zero(m);
    m.type = SDL_MOUSEBUTTONDOWN;
    m.button.button = SDL_BUTTON_LEFT;
    m.button.which = 0;
    m.button.windowID = SDL_GetWindowID(window);
    m.button.x = (int)bx;
    m.button.y = (int)by;
    assert(pushed_point(&m, &x, &y));
    if (wing_button_at(x, y) != b) {
      fprintf(stderr, "%dx%d %s: a click on button %d at %.0f,%.0f came in at %.0f,%.0f\n", ww, wh, setup, b, bx, by, x,
              y);
      assert(0);
    }
    // SDL's mouse copy of a touch is not a click of its own.
    m.button.which = SDL_TOUCH_MOUSEID;
    assert(!pushed_point(&m, &x, &y));
    checks++;
  }
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  // The OneXPlayer's 1920x1200 touch screen, other handhelds' and desktops', and odd window sizes.
  static const int sizes[][2] = {{1920, 1200}, {1280, 800}, {2560, 1600}, {1920, 1080}, {1280, 720}, {800, 480},
                                 {1366, 768}, {1037, 611}};
  for (size_t s = 0; s < sizeof sizes / sizeof *sizes; s++) {
    int ww = sizes[s][0], wh = sizes[s][1];
    window = SDL_CreateWindow("touch", 0, 0, ww, wh, 0);
    assert(window);
    renderer = make_renderer(window, 0);
    assert(renderer);
    // The right wing at each wing size the UI SIZE and WING ROOM give (an eighth to a quarter of the width).
    for (int part = 4; part <= 8; part += 2) {
      int wingW = ww / part;
      for (int canvasW = 120; canvasW <= 200; canvasW += 40) {
        int canvasH = canvasW * wh / wingW;
        wing_at((SDL_Rect){ww - wingW, 0, wingW, wh}, canvasW, canvasH);
        // What the host leaves the renderer with: no logical size, the game letterboxed in its own, a 16:9 canvas,
        // a whole-number scale, and a scale of 1.5 (a 150% screen's).
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        SDL_RenderSetScale(renderer, 1, 1);
        check_buttons(ww, wh, "no logical size");
        SDL_RenderSetLogicalSize(renderer, 240, 160);
        SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
        check_buttons(ww, wh, "the game's 240x160, whole scale");
        SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
        SDL_RenderSetLogicalSize(renderer, 1280, 720);
        check_buttons(ww, wh, "1280x720");
        SDL_RenderSetLogicalSize(renderer, 480, 320);
        check_buttons(ww, wh, "480x320");
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        SDL_RenderSetScale(renderer, 1.5f, 1.5f);
        check_buttons(ww, wh, "scale 1.5");
        SDL_RenderSetScale(renderer, 1, 1);
      }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    renderer = NULL;
    window = NULL;
  }
  printf("touch: %d button checks, each by finger and mouse\n", checks);
  SDL_Quit();
  return 0;
}
