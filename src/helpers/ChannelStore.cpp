#include "ChannelStore.h"
#include <helpers/IdentityStore.h>
#include <helpers/TxtDataHelpers.h>
#include <Utils.h>

#ifdef MAX_GROUP_CHANNELS

static File openWriteChannels(FILESYSTEM* _fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  _fs->remove(filename);
  return _fs->open(filename, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return _fs->open(filename, "w");
#else
  return _fs->open(filename, "w", true);
#endif
}

void ChannelStore::load(FILESYSTEM* fs) {
  _fs = fs;
  _num_channels = 0;
  if (!_fs) return;
  if (!_fs->exists(CHANNELS_FILE)) return;

#if defined(RP2040_PLATFORM)
  File file = _fs->open(CHANNELS_FILE, "r");
#else
  File file = _fs->open(CHANNELS_FILE);
#endif
  if (file) {
    while (1) {
      char name[32];
      uint8_t secret[32];
      uint8_t flags = 0;
      uint8_t pad[3];
      uint32_t created = 0;

      int r = file.read((uint8_t*)name, sizeof(name));
      if (r != (int)sizeof(name)) break;
      if (file.read(secret, sizeof(secret)) != (int)sizeof(secret)) break;
      if (file.read(&flags, 1) != 1) break;
      if (file.read(pad, 3) != 3) break;
      if (file.read((uint8_t*)&created, 4) != 4) break;

      // store
      int idx = _num_channels;
      if (idx < MAX_GROUP_CHANNELS) {
        ChannelDetails& cd = _channels[idx];
        memset(&cd, 0, sizeof(cd));
        memcpy(cd.channel.secret, secret, sizeof(secret));
        mesh::Utils::sha256(cd.channel.hash, sizeof(cd.channel.hash), cd.channel.secret, sizeof(cd.channel.secret));
        StrHelper::strncpy(cd.name, name, sizeof(cd.name));
        _flags[idx] = flags;
        _created_at[idx] = created;
        _num_channels++;
      } else {
        // ignore extra entries
      }
    }
    file.close();
  }
}

void ChannelStore::save(FILESYSTEM* fs) {
  _fs = fs;
  if (!_fs) return;
  File file = openWriteChannels(_fs, CHANNELS_FILE);
  if (!file) return;

  for (int i = 0; i < _num_channels; i++) {
    const ChannelDetails& cd = _channels[i];
    char name[32];
    memset(name, 0, sizeof(name));
    StrHelper::strncpy(name, cd.name, sizeof(name));
    file.write((const uint8_t*)name, sizeof(name));
    file.write(cd.channel.secret, sizeof(cd.channel.secret));
    file.write(&_flags[i], 1);
    uint8_t pad[3] = {0,0,0}; file.write(pad, 3);
    uint32_t created = _created_at[i];
    file.write((const uint8_t*)&created, 4);
  }
  file.close();
}

int ChannelStore::findByNamePrefix(const char* name) {
  if (!name || !*name) return -1;
  const char* nm = name;
  if (nm[0] == '#') nm++; // allow leading '#'
  for (int i = 0; i < _num_channels; i++) {
    if (StrHelper::startsWith(_channels[i].name, nm)) return i;
  }
  return -1;
}

int ChannelStore::addChannel(const char* name, const uint8_t* key, int key_len, uint8_t flags) {
  if (!name || !*name) return -1;
  if (_num_channels >= MAX_GROUP_CHANNELS) return -1;
  int idx = _num_channels;
  ChannelDetails& cd = _channels[idx];
  memset(&cd, 0, sizeof(cd));
  StrHelper::strncpy(cd.name, (name[0]=='#')?&name[1]:name, sizeof(cd.name));
  memset(cd.channel.secret, 0, sizeof(cd.channel.secret));
  if (key && (key_len == 16 || key_len == 32)) {
    memcpy(cd.channel.secret, key, key_len);
  }
  mesh::Utils::sha256(cd.channel.hash, sizeof(cd.channel.hash), cd.channel.secret, 32);
  _flags[idx] = flags;
  _created_at[idx] = 0;
  _num_channels++;
  return idx;
}

bool ChannelStore::removeChannelByIdx(int idx) {
  if (idx < 0 || idx >= _num_channels) return false;
  for (int i = idx; i + 1 < _num_channels; i++) {
    _channels[i] = _channels[i+1];
    _flags[i] = _flags[i+1];
    _created_at[i] = _created_at[i+1];
  }
  _num_channels--;
  return true;
}

void ChannelStore::exportTo(char* out, int out_size) {
  if (!out || out_size <= 0) return;
  int used = 0;
  used += snprintf(out + used, max(0, out_size - used), "%d channels\n", _num_channels);
  for (int i = 0; i < _num_channels && used < out_size - 10; i++) {
    used += snprintf(out + used, max(0, out_size - used), "%d %s\n", i, _channels[i].name);
  }
  if (used < out_size) out[used] = 0;
}

#endif
