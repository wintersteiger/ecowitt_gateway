// Copyright (c) Christoph M. Wintersteiger
// Licensed under the MIT License.

#include <cmath>
#include <string>
#include <memory>
#include <fstream>

#include <pqxx/pqxx>
#include <pqxx/except.hxx>

#include <serialization.h>
#include "pqarchive.h"

inline std::string address_to_string(uint64_t addr)
{
  std::string r;
  r.reserve(sizeof(addr) * 3);
  for (size_t i=0; i < sizeof(addr); i++) {
    uint8_t byte_i = addr >> (8*(sizeof(addr)-1-i));
    char tmp[4];
    snprintf(tmp, sizeof(tmp), "%s%02x", (i == 0 ? "" : ":"), byte_i);
    r += tmp;
  }
  return r;
}

PQArchive::PQArchive(const std::string &connection_string) :
  connection_string(connection_string)
{
  connect();
}

void PQArchive::connect()
{
  {
    pqc = std::make_unique<pqxx::connection>(connection_string);

    execute("CREATE TABLE IF NOT EXISTS names (address MACADDR8 NOT NULL UNIQUE, name TEXT NOT NULL);");

    execute("CREATE TABLE IF NOT EXISTS ecowitt_frames (time TIMESTAMP NOT NULL, "
                    "address MACADDR8 NOT NULL, "
                    "rssi FLOAT(24) NOT NULL,"
                    "frame BYTEA NOT NULL);");
    execute("CREATE INDEX IF NOT EXISTS ecowitt_frames_time_idx ON ecowitt_frames (time DESC);");
    execute("SELECT create_hypertable('ecowitt_frames', 'time', if_not_exists => TRUE);", true);

    execute("CREATE TABLE IF NOT EXISTS soil_moisture (time TIMESTAMP NOT NULL, "
                    "address MACADDR8 NOT NULL, "
                    "value FLOAT(24) NOT NULL);");
    execute("CREATE INDEX IF NOT EXISTS soil_moisture_time_idx ON soil_moisture (time DESC);");
    execute("SELECT create_hypertable('soil_moisture', 'time', if_not_exists => TRUE);", true);

    execute("CREATE TABLE IF NOT EXISTS battery_voltage (time TIMESTAMP NOT NULL, "
                    "address MACADDR8 NOT NULL, "
                    "value FLOAT(24) NOT NULL);");
    execute("CREATE INDEX IF NOT EXISTS battery_voltage_time_idx ON battery_voltage (time DESC);");
    execute("SELECT create_hypertable('battery_voltage','time', if_not_exists => TRUE);", true);
  }

  {
    std::ifstream f(local_backlog);
    std::string line;
    while (std::getline(f, line))
    {
      pqxx::work txn{*pqc};
      txn.exec(line);
      txn.commit();
    }
  }

  {
    std::ifstream f(local_backlog, std::ios::trunc);
    f.close();
  }
}

void PQArchive::set_device_name(uint64_t addr, const std::string &name)
{
  execute("INSERT INTO names VALUES ('" + address_to_string(addr) + "', '" + name + "') "
          "ON CONFLICT (address) DO UPDATE SET name=excluded.name;");
}

void PQArchive::insert_ecowitt_frame(time_t tme, uint64_t addr, float rssi, const std::vector<uint8_t> &frame)
{
  std::string tme_str = "to_timestamp(" + std::to_string(tme) + ")";
  auto hex_frame = to_hex(frame);
  execute("INSERT INTO ecowitt_frames VALUES (" + tme_str + ", '" + address_to_string(addr) + "', " + std::to_string(rssi) + ",'\\x" + hex_frame + "');");
}

void PQArchive::insert_soil_moisture(time_t tme, uint64_t addr, float value)
{
  std::string tme_str = "to_timestamp(" + std::to_string(tme) + ")";
  auto v = std::isnan(value) ? "'NaN'" : std::to_string(value);
  execute("INSERT INTO soil_moisture VALUES (" + tme_str + ", '" + address_to_string(addr) + "', " + v + ");");
}

void PQArchive::insert_battery_voltage(time_t tme, uint64_t addr, float value)
{
  std::string tme_str = "to_timestamp(" + std::to_string(tme) + ")";
  auto v = std::isnan(value) ? "'NaN'" : std::to_string(value);
  execute("INSERT INTO battery_voltage VALUES (" + tme_str + ", '" + address_to_string(addr) + "', " + v + ");");
}

void PQArchive::execute(const std::string &query, bool exec1)
{
  const std::lock_guard<std::mutex> lock(mtx);

  if (!pqc) connect();

  try {
    pqxx::work txn{*pqc};
    if (exec1)
      txn.exec1(query);
    else
      txn.exec0(query);
    txn.commit();
  }
  catch (const std::exception &ex) {
    to_backlog(query);
  }
  catch (...) {
    to_backlog(query);
  }
}

void PQArchive::to_backlog(const std::string &query) {
  std::ofstream f(local_backlog, std::ios::app);
  if (f)
    f << query << std::endl;
}