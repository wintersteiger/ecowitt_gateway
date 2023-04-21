// Copyright (c) Christoph M. Wintersteiger
// Licensed under the MIT License.

#include <sstream>
#include <iomanip>

#include "ecowitt_wh51_ui.h"

Ecowitt_WH51_UI::Ecowitt_WH51_UI(uint32_t id, Ecowitt_WH51& wh51) :
  wh51(wh51)
{
  size_t row = 1, col = 1;

  std::stringstream wheress;
  wheress << wh51.where() << " (0x" << std::hex << std::setw(2) << std::setfill('0') << (int)id << ")";

  Add(new TimeField(UI::statusp, row, col));
  Add(new Label(UI::statusp, row++, col + 18, Name().c_str()));
  Add(new Label(UI::statusp, row++, col, wheress.str().c_str()));
  Add(new Empty(row++, col));

  float battery_voltage;
  uint8_t moisture;
  uint16_t raw;
  Add(new LField<uint8_t>(UI::statusp, row++, col, 10, "Fast updates", "", [&wh51]() -> uint8_t {
    return wh51.state.fast_update_periods;
  }));
  Add(new LField<float>(UI::statusp, row++, col, 10, "Batt. voltage", "V", [&wh51]() -> float {
    return wh51.state.battery_voltage;
  }));
  Add(new LField<float>(UI::statusp, row++, col, 10, "Moisture", "%", [&wh51]() -> float {
    return wh51.state.moisture;
  }));
  Add(new LField<uint16_t>(UI::statusp, row++, col, 10, "Raw", "", [&wh51]() -> uint16_t {
    return wh51.state.raw;
  }));

  Add(new Empty(row++, col));

  Add(new LField<const char*>(UI::statusp, row++, col, 10, "Last seen date", "", [&wh51]() {
    static char tmp[1024] = "";
    struct tm * lt = localtime(&wh51.state.last_seen);
    strftime(tmp, sizeof(tmp), "%Y%m%d", lt);
    return tmp;
  }));
  Add(new LField<const char*>(UI::statusp, row++, col, 10, "Last seen time", "", [&wh51]() {
    static char tmp[1024] = "";
    struct tm * lt = localtime(&wh51.state.last_seen);
    strftime(tmp, sizeof(tmp), "%H:%M:%S", lt);
    return tmp;
  }));
}
