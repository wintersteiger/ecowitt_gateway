// Copyright (c) Christoph M. Wintersteiger
// Licensed under the MIT License.

#ifndef _ECOWITT_WH51_UI_H_
#define _ECOWITT_WH51_UI_H_

#include <memory>

#include <ui.h>

#include "ecowitt_wh51.h"

class Ecowitt_WH51_UI : public UI
{
public:
  Ecowitt_WH51_UI(uint32_t id, Ecowitt_WH51& wh51);
  virtual ~Ecowitt_WH51_UI() = default;

  virtual std::string Name() const override { return "Ecowitt WH51"; }

protected:
  std::string where;
  Ecowitt_WH51& wh51;
};

#endif // _ECOWITT_WH51_UI_H_
