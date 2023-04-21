// Copyright (c) Christoph M. Wintersteiger
// Licensed under the MIT License.

#ifndef _REMOTE_DEVICE_STATE_H_
#define _REMOTE_DEVICE_STATE_H_

template <typename STATE_TYPE>
class RemoteDevice {
public:
  RemoteDevice() {}
  virtual ~RemoteDevice() {}

  STATE_TYPE state;
};

#endif // _REMOTE_DEVICE_STATE_H_
