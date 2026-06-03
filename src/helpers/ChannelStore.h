#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/ChannelDetails.h>

// Include IdentityStore so FILESYSTEM is defined appropriately for the platform
#include <helpers/IdentityStore.h>

#ifndef CHANNELS_FILE
#define CHANNELS_FILE "/s_channels"
#endif

#ifdef MAX_GROUP_CHANNELS

class ChannelStore {
  FILESYSTEM* _fs;
  ChannelDetails _channels[MAX_GROUP_CHANNELS];
  uint8_t _flags[MAX_GROUP_CHANNELS];
  uint32_t _created_at[MAX_GROUP_CHANNELS];
  int _num_channels;

public:
  ChannelStore() { _fs = NULL; _num_channels = 0; memset(_flags, 0, sizeof(_flags)); memset(_created_at, 0, sizeof(_created_at)); }

  void load(FILESYSTEM* fs);
  void save(FILESYSTEM* fs);

  int getNumChannels() const { return _num_channels; }
  const ChannelDetails* getChannel(int idx) const { return (idx >= 0 && idx < _num_channels) ? &_channels[idx] : NULL; }
  uint8_t getFlags(int idx) const { return (idx >= 0 && idx < _num_channels) ? _flags[idx] : 0; }

  int findByNamePrefix(const char* name);
  int addChannel(const char* name, const uint8_t* key, int key_len, uint8_t flags);
  bool removeChannelByIdx(int idx);

  void exportTo(char* out, int out_size);
  bool isPublic(int idx) const { return (getFlags(idx) & 0x01) != 0; }
};

#else

class ChannelStore {
public:
  ChannelStore() {}
  void load(FILESYSTEM*) {}
  void save(FILESYSTEM*) {}
  int getNumChannels() const { return 0; }
  const ChannelDetails* getChannel(int) const { return NULL; }
  int findByNamePrefix(const char*) { return -1; }
  int addChannel(const char*, const uint8_t*, int, uint8_t) { return -1; }
  bool removeChannelByIdx(int) { return false; }
  void exportTo(char* out, int out_size) { strncpy(out, "Channels unsupported", out_size-1); out[out_size-1]=0; }
  bool isPublic(int) const { return false; }
};

#endif
