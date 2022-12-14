#include "mixer_gui.h"
#include "comm_api.h"
#include "gfx.h"
#include "src/gwin/gwin_class.h"
#include <array>
#include <optional>

static constexpr uint32_t MAX_LINES = 5;
static void gui_redraw();

static CommAPI& api = CommAPI::get_instance();

static gFont font;

/// @brief Used to render one "line" on the GUI
struct SetVolumeHelper {
  using img_data_t = std::array<uint8_t, 5500>;
  /// @brief Line is set at creation, and not changed after
  /// @param line line of this instance
  SetVolumeHelper(int line) : line_(line) {
  }

  /// @brief Create the widgets, hidden
  void init() {
    GWidgetInit wi;
    gwinWidgetClearInit(&wi);
    wi.g.show = gFalse;
    wi.g.x = base_x;
    wi.g.y = base_y + line_ * multiplier;
    wi.g.height = 32;
    wi.g.width = 32;

    img_handle_ = gwinImageCreate(0, &wi.g);
    // gwinSetBgColor(img_handle_, GFX_BLACK);

    wi.g.x += multiplier;
    wi.text = "M";
    btn_mute_ = gwinButtonCreate(0, &wi);


    wi.g.x += multiplier;
    wi.text = "-";
    btn_minus_ = gwinButtonCreate(0, &wi);


    wi.g.x += multiplier;
    wi.g.width = gdispGetWidth() - base_x - 3 * multiplier - base_x - multiplier;
    wi.text = "%";
    slider_ = gwinSliderCreate(0, &wi);

    wi.text = "+";
    wi.g.width = 32;
    wi.g.x = gdispGetWidth() - base_x - 32;
    btn_plus_ = gwinButtonCreate(0, &wi);
  }

  /// @brief Render the line, if it's set
  /// @details Only redraw parts, which have changed since last call
  void render() {
    if (not volume_) {
      return;
    }
    auto curr = *volume_;  // needed for debug, pio doesnt work with optional :/

    if (volume_changed_) {
      volume_changed_ = false;
      show_widgets();
      if (curr.muted_) {
        snprintf(slider_txt_.data(), slider_txt_.size() - 1, "Mute (%d%%)", curr.volume_);
      } else {
        snprintf(slider_txt_.data(), slider_txt_.size() - 1, "%d%%", curr.volume_);
      }
      gwinSliderSetPosition(slider_, curr.volume_);
      gwinSetText(slider_, slider_txt_.data(), gFalse);
    }

    if (session_change_) {
      if (0 != api.load_image(curr.pid_, img_data_.data(), img_data_.size())) {
        return;
      }
      gwinClear(img_handle_);
      if (gTrue != gwinImageOpenMemory(img_handle_, img_data_.data())) {
        return;
      }
      // all success, dont redraw next time
      session_change_ = false;
    }
  }

  /// @brief Set the session of this line
  void set_volume(const mixer::ProgramVolume& vol) {
    if (volume_ && volume_->pid_ == vol.pid_) {
      // picture haven't changed
      // session_change_ = false;
      volume_changed_ = volume_changed_ || (vol.volume_ != volume_->volume_) || (vol.muted_ != volume_->muted_);
    } else {
      session_change_ = true;
      volume_changed_ = true;
    }
    volume_ = vol;
  }

  /// @brief Hide this line and remove session info
  void reset() {
    volume_ = std::nullopt;
    hide_widgets();
  }

  /// @brief Check if event belongs to this line and act accordingly
  void handle_event(const GEvent* ev) {
    if (not volume_) {
      return;
    }

    if (ev->type == GEVENT_GWIN_BUTTON) {
      const auto handle = ((GEventGWinButton*)ev)->gwin;
      handle_button_event(handle);
    }

    if (ev->type == GEVENT_GWIN_SLIDER) {
      handle_slider_event((GEventGWinSlider*)ev);
    }
  }

private:
  void hide_widgets() {
    gwinHide(img_handle_);
    gwinHide(btn_mute_);
    gwinHide(btn_minus_);
    gwinHide(btn_plus_);
    gwinHide(slider_);
    gwinHide(img_handle_);
  }

  void show_widgets() {
    gwinShow(img_handle_);
    gwinShow(btn_mute_);
    gwinShow(btn_minus_);
    gwinShow(btn_plus_);
    gwinShow(slider_);
  }

  void handle_button_event(const GHandle h) {
    handle_mute_btn(h);
    handle_plus_minus_btn(h);
  }

  void handle_plus_minus_btn(const GHandle h) {
    if (h != btn_minus_ && h != btn_plus_) {
      return;
    }

    bool need_change = false;

    int16_t vol = volume_->volume_;  // so no underflow when minus
    // finer control for master volume
    const int16_t increment = volume_->pid_ == -1 ? 2 : 10;
    if (h == btn_minus_) {
      need_change = true;
      vol -= increment;
    } else if (h == btn_plus_) {
      need_change = true;
      vol += increment;
    }

    if (need_change) {
      api.set_volume(volume_->pid_, utils::constrain(vol, 0, 100));
    }
  }

  void handle_mute_btn(const GHandle h) {
    if (h != btn_mute_) {
      return;
    }
    api.set_mute(volume_->pid_, not volume_->muted_);
  }

  void handle_slider_event(const GEventGWinSlider* ev) {
    if (ev->gwin == slider_ && ev->action == GSLIDER_EVENT_SET) {
      api.set_volume(volume_->pid_, utils::constrain(ev->position, 0, 100));
    }
  }

  GHandle img_handle_{};
  GHandle btn_plus_{};
  GHandle btn_minus_{};
  GHandle btn_mute_{};
  GHandle slider_{};
  const int line_;
  std::optional<mixer::ProgramVolume> volume_;
  bool session_change_ = true;               ///< If true, picture will be redrawn
  bool volume_changed_ = true;               ///< If true, slider is redrawn
  std::array<char, 30> slider_txt_ = { 0 };  ///< holds the text on the slider

  static constexpr unsigned base_x = 10;      ///< X of first widget
  static constexpr unsigned base_y = 10;      ///< Y of first widget
  static constexpr unsigned multiplier = 40;  ///< spacing of widgets

  static img_data_t img_data_;  ///< Only one storage is enough, since we dont redraw frequently
};

SetVolumeHelper::img_data_t SetVolumeHelper::img_data_;

std::array<SetVolumeHelper, MAX_LINES> gui_objs = { SetVolumeHelper(0), SetVolumeHelper(1), SetVolumeHelper(2),
                                                    SetVolumeHelper(3), SetVolumeHelper(4) };



enum gui_state_t : uint8_t {
  WAKEUP,
  DRAW,
  IDLE,
  GOSLEEP,
  SLEEPING,
  NUM_STATES,
};

enum gui_event : uint8_t {
  NOEVENT,
  UI_INPUT,
  CHANGE,
  SERIAL_ERROR,
  SERIAL_TIMEOUT,
  NUM_EVENTS,
};

/// @brief 2D array of size NUM_STATES x NUM_EVENTS, where index [i][j] contains the state to go to from state \e i on
/// event \e j
using transition_map_t = std::array<std::array<gui_state_t, NUM_EVENTS>, NUM_STATES>;

static constexpr std::array<gui_state_t, NUM_EVENTS> filled_event_arr(gui_state_t e) {
  std::array<gui_state_t, NUM_EVENTS> arr{};
  for (int i = 0; i < NUM_EVENTS; ++i) {
    arr[i] = e;
  }
  return arr;
}

static constexpr transition_map_t create_transitions() {
  transition_map_t t{};

  // from always WAKEUP go to DRAW
  t[WAKEUP] = filled_event_arr(DRAW);
  // from DRAW default to IDLE, except...
  t[DRAW] = filled_event_arr(IDLE);
  t[DRAW][CHANGE] = DRAW;
  t[DRAW][UI_INPUT] = DRAW;
  // from IDLE ...
  t[IDLE] = filled_event_arr(IDLE);
  t[IDLE][CHANGE] = DRAW;
  t[IDLE][UI_INPUT] = DRAW;
  t[IDLE][SERIAL_TIMEOUT] = GOSLEEP;
  // from GOSLEEP
  t[GOSLEEP] = filled_event_arr(SLEEPING);
  t[GOSLEEP][CHANGE] = WAKEUP;
  // from SLEEPING
  t[SLEEPING] = filled_event_arr(SLEEPING);
  t[SLEEPING][CHANGE] = WAKEUP;

  return t;
}

static constexpr auto transitions = create_transitions();


void mixer_gui_task() {
  font = gdispOpenFont("DejaVuSans12*");
  gwinSetDefaultStyle(&BlackWidgetStyle, false);
  gwinSetDefaultFont(font);


  GListener gl;
  geventListenerInit(&gl);
  gwinAttachListener(&gl);

  // create the widgets
  for (auto& helper : gui_objs) {
    helper.init();
  }

  gui_state_t state = gui_state_t::WAKEUP;

  while (1) {
    // gather events
    gui_event event = gui_event::NOEVENT;

    // wait for user input, no timeout if drawing
    const gDelay timeout = state == DRAW ? 0 : 1000;
    GEvent* pe = geventEventWait(&gl, timeout);

    if (pe) {
      event = gui_event::UI_INPUT;
      // if input, handle it in widgets
      for (auto& obj : gui_objs) {
        obj.handle_event(pe);
      }
    }

    switch (api.changes()) {
      case 0:  // new changes in volumes
        event = gui_event::CHANGE;
        break;
      case 1:  // no new changes
        break;
      case 2:  // comm failure
        event = gui_event::SERIAL_ERROR;
        if (api.since_last_success() > (30 * 1000)) {
          // 30 seconds elapsed since last successful message
          event = gui_event::SERIAL_TIMEOUT;
        }
        break;
      default:
        break;
    }

    // do the state machine
    switch (state) {
      case gui_state_t::WAKEUP:
        // turn on backlight
        gdispSetBacklight(100);
        break;

      case gui_state_t::DRAW:
        gui_redraw();
        break;

      case gui_state_t::IDLE:
        break;

      case gui_state_t::GOSLEEP:
        gdispSetBacklight(0);
        break;

      case gui_state_t::SLEEPING:
        xTaskNotifyWait(0, UINT32_MAX, nullptr, pdMS_TO_TICKS(60 * 1000));
        break;

      case gui_state_t::NUM_STATES:
        break;
    }

    // transition
    state = transitions[state][event];
  }
}



static void gui_redraw() {
  // something is new, load the volumes
  if (CommAPI::ret_t::OK != api.load_volumes() || (not api.get_volumes()[0])) {
    return;
  }

  // draw the volumes one by one
  unsigned line = 0;
  for (const auto& vol : api.get_volumes()) {
    if (vol) {
      auto& curr = gui_objs[line];
      ++line;
      curr.set_volume(*vol);
      curr.render();
    }
  }
  // clear the rest of the lines
  for (; line < gui_objs.size(); ++line) {
    gui_objs[line].reset();
  }
}