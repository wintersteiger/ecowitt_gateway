// Copyright (c) Christoph M. Wintersteiger
// Licensed under the MIT License.

#include "ecowitt_wh51.h"

Ecowitt_WH51::Ecowitt_WH51(const std::string &where) :
  RemoteDevice<Ecowitt_WH51_State>(),
  where_(where)
{}

Ecowitt_WH51::Ecowitt_WH51(const nlohmann::json &j) :
  RemoteDevice<Ecowitt_WH51_State>()
{
  auto q = j.get<Ecowitt_WH51>();
  where_ = q.where_;
  state = q.state;
}

Ecowitt_WH51::operator nlohmann::json() const
{
  return *this;
}
