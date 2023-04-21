// Copyright (c) Christoph M. Wintersteiger
// Licensed under the MIT License.

#ifndef _ECOWITT_WH51_H_
#define _ECOWITT_WH51_H_

#include <vector>
#include <string>

#include <json.hpp>

#include "remote_device.h"

struct Ecowitt_WH51_State {
  uint8_t fast_update_periods = 0;
  float battery_voltage = 0.0;
  uint8_t moisture = 0;
  uint16_t raw = 0;

  time_t last_seen = 0;
  double last_rssi = -256.0;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Ecowitt_WH51_State, fast_update_periods, battery_voltage, moisture, raw, last_seen, last_rssi);
};

class Ecowitt_WH51 : public RemoteDevice<Ecowitt_WH51_State>
{
public:
  Ecowitt_WH51(const std::string &where = "");
  Ecowitt_WH51(const nlohmann::json &j);
  virtual ~Ecowitt_WH51() {}

  using RemoteDevice::state;

  operator nlohmann::json() const;

  const std::string& where() const { return where_; }

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Ecowitt_WH51, state, where_);

protected:
  std::string where_;
};

#endif // _ECOWITT_WH51_H_
