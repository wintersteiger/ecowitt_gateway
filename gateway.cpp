// Copyright (c) Christoph M. Wintersteiger
// Licensed under the MIT License.

#include <cinttypes>
#include <iomanip>
#include <vector>
#include <iostream>
#include <fstream>
#include <mutex>
#include <memory>
#include <map>
#include <random>

#include <shell.h>
#include <serialization.h>
#include <ui.h>
#include <sleep.h>
#include <integrity.h>
#include <gpio_button.h>
#include <json.hpp>
#include <gpio_watcher.h>

#ifdef USE_C1101
#include <cc1101.h>
#include <cc1101_ui.h>
#include <cc1101_ui_raw.h>
#else
#include <spirit1.h>
#include <spirit1_ui.h>
#endif

#include "ecowitt_wh51.h"
#include "ecowitt_wh51_ui.h"
#include "pqarchive.h"

static std::mutex mtx;
static std::shared_ptr<Shell> shell;
static uint64_t irqs = 0, num_manual = 0;

struct Devices {
  std::map<uint32_t, Ecowitt_WH51> wh51s;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Devices, wh51s);
};

static Devices devices;

static std::shared_ptr<PQArchive> pqarchive;

class Cache {
public:
  Cache(const std::string &cache_file_name, Devices &devices) :
    cache_file_name(cache_file_name),
    devices(devices)
  {
    std::ifstream s(cache_file_name);
    if (s) {
      nlohmann::json j;
      s >> j;
      devices = j;
    }
  }

  virtual ~Cache() {
    std::ofstream s(cache_file_name);
    if (s)
      s << (nlohmann::json)(devices);
  }

protected:
  Devices &devices;
  std::string cache_file_name;
};

std::pair<uint32_t, const Ecowitt_WH51_State*> decode(double rssi, std::vector<uint8_t> &bytes)
{
  if (bytes.size() < 2)
    throw std::runtime_error("packet too small");

  if (checksum(bytes, true) != bytes.back())
    throw std::runtime_error("checksum failure");

  bytes.pop_back();

  if (crc8(bytes, 0x31, true) != bytes.back())
    throw std::runtime_error("crc8 failure");

  if (bytes[0] != 0x51)
    throw std::runtime_error("unknown family code");

  uint32_t id = bytes[1] << 16 | bytes[2] << 8 | bytes[3];

  auto dit = devices.wh51s.find(id);
  if (dit == devices.wh51s.end())
    throw std::runtime_error("unknown device id");

  auto& dev = dit->second;

  dev.state.last_seen = time(NULL);
  dev.state.last_rssi = rssi;
  dev.state.fast_update_periods = bytes[4] >> 4;
  dev.state.battery_voltage = ((uint8_t)bytes[4] & 0x0F) / 10.0f;
  dev.state.moisture = bytes[6];
  dev.state.raw = (bytes[7] & 0x01) << 8 | bytes[8];

  return {id, &dev.state};
}

static bool fRX(std::shared_ptr<Radio> radio)
{
  const std::lock_guard<std::mutex> lock(mtx);

  std::vector<uint8_t> packet;
  radio->Receive(packet);

  double rssi = radio->RSSI();
  auto pkt_hex = to_hex(packet);

  try {
    auto id_state = decode(rssi, packet);
    UI::Log("RX: rssi=%0.2f: %s", rssi, pkt_hex.c_str());
    auto state = *id_state.second;

    pqarchive->insert_ecowitt_frame(state.last_seen, id_state.first, state.last_rssi, packet);
    pqarchive->insert_soil_moisture(state.last_seen, id_state.first, state.moisture);
    pqarchive->insert_battery_voltage(state.last_seen, id_state.first, state.battery_voltage);
  }
  catch (const std::runtime_error &ex) {
    UI::Log("RX: rssi=%0.2f: %s: Error: %s", rssi, pkt_hex.c_str(), ex.what());
  }

  radio->Goto(Radio::State::RX);

  return true;
}

static bool fIRQ(std::shared_ptr<Radio> radio)
{
  irqs = radio->IRQHandler();
  if (shell)
    shell->controller->Update(false);
  if (irqs & 0x00000201)
    return fRX(radio);
  return true;
}

int main()
{
  UI::Start();
  UI::SetLogFile("ui.log");

  pqarchive = std::make_shared<PQArchive>("postgresql://cwinter:UxTCFqbq4Bd3xT1Mq3vC@192.168.0.41/station_house");
  pqarchive->set_device_name(0x00EE58, "Garden");
  pqarchive->set_device_name(0x00EF32, "A10");

  try {
    shell = get_shell(1.0);

#ifdef USE_C1101
    auto radio = std::make_shared<CC1101>(0, 0, "cc1101.cfg");
    auto radio_ui = std::make_shared<CC1101UI>(radio);
    auto radio_ui_raw = make_cc1101_raw_ui(radio);
#else
    auto reset_button = std::make_shared<GPIOButton>("/dev/gpiochip0", 5);
    reset_button->Write(true);
    sleep_ms(2);
    reset_button->Write(false);
    sleep_ms(2);
    auto radio = std::make_shared<SPIRIT1>(0, 0, "spirit1.cfg");
    auto radio_ui = std::make_shared<SPIRIT1UI>(radio, irqs);
    auto radio_ui_raw = make_spirit1_raw_ui(radio, reset_button);
#endif

    std::vector<std::shared_ptr<DeviceBase>> radio_devs = {radio};

    std::vector<GPIOWatcher<Radio>*> gpio_watchers;
#ifdef USE_C1101
    gpio_watchers.push_back(new GPIOWatcher<Radio>("/dev/gpiochip0", 25, "WLMCD-CC1101", radio, true,
      [](int, unsigned, const timespec*, std::shared_ptr<Radio> radio) {
        return fRX(radio);
      }));
#else
    gpio_watchers.push_back(new GPIOWatcher<Radio>("/dev/gpiochip0", 25, "WLMCD-SPIRIT1", radio, false,
      [](int, unsigned, const timespec*, std::shared_ptr<Radio> radio) {
        return fIRQ(radio);
      }));
#endif

    Cache c("gateway.cache.json", devices);
    // devices.wh51s.emplace(0x00EE58, Ecowitt_WH51((std::string)"Garden"));
    // devices.wh51s.emplace(0x00EF32, Ecowitt_WH51((std::string)"A10"));

    shell->controller->AddBackgroundDevice(std::make_shared<BackgroundTask>([radio]() {
      if (mtx.try_lock()) {
        if (radio) {
          if (radio->RXReady()) {
            mtx.unlock();
            fRX(radio);
            num_manual++;
          }
          else {
            radio->Goto(Radio::State::RX);
            mtx.unlock();
          }
        }
      }
    }));

    shell->controller->AddCommand("crc8", [](const std::string &args){
      auto argbytes = from_hex(args);
      auto x = crc8(argbytes, 0x07);
      UI::Info("crc8 is %02x", x);
    });

    shell->controller->AddCommand("checksum", [](const std::string &args){
      auto argbytes = from_hex(args);
      auto x = checksum(argbytes);
      UI::Info("checksum is %02x", x);
    });

    shell->controller->AddCommand("checkxor", [](const std::string &args){
      auto argbytes = from_hex(args);
      auto x = checkxor(argbytes);
      UI::Info("checkxor is %02x", x);
    });

    shell->controller->AddCommand("run", [radio](const std::string &args){
      radio->Goto(Radio::State::RX);
    });

    shell->controller->AddCommand("rx", [radio](const std::string &args){
      return fRX(radio);
    });

    for (auto &id_dev : devices.wh51s)
      shell->controller->AddSystem(std::make_shared<Ecowitt_WH51_UI>(id_dev.first, id_dev.second));

    shell->controller->AddSystem(radio_ui);
    shell->controller->AddSystem(radio_ui_raw);

    radio->Goto(Radio::State::RX);

    shell->controller->Run();

    return shell->exit_code;
  }
  catch (std::exception &ex) {
    UI::End();
    std::cout << "Exception: " << ex.what() << std::endl;
    return 1;
  }
  catch (...) {
    UI::End();
    std::cout << "Caught unknown exception." << std::endl;
    return 1;
  }
}
