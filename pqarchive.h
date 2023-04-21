// Copyright (c) Christoph M. Wintersteiger
// Licensed under the MIT License.

#ifndef _PQ_ARCHIVE_H_
#define _PQ_ARCHIVE_H_

#include <memory>
#include <string>
#include <mutex>

#include <pqxx/pqxx>

class PQArchive
{
public:
  PQArchive(const std::string &connection_string = "");
  virtual ~PQArchive() = default;

  bool enabled() const { return connection_string != ""; }

  void insert_ecowitt_frame(time_t tme, uint64_t addr, float rssi, const std::vector<uint8_t> &frame);
  void insert_soil_moisture(time_t tme, uint64_t addr, float value);
  void insert_battery_voltage(time_t tme, uint64_t addr, float value);

  void set_device_name(uint64_t addr, const std::string &name);

protected:
  std::unique_ptr<pqxx::connection> pqc;
  std::string connection_string;
  std::string local_backlog = "pqarchive.log";
  mutable std::mutex mtx;

  void connect();
  void create_table(const std::string &name, const std::string &value_type = "TEXT");
  void execute(const std::string &query, bool exec1 = false);
  void to_backlog(const std::string &query);
};

#endif // _PQ_ARCHIVE_H_
