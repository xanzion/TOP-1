#pragma once

#include <type_traits>

#include "top1file.h"

namespace top1 {

template<typename sample_type, uint _channels>
class BasicSndFile : public File {
protected:

  struct WavHeader : public Chunk {
    ChunkFCC format = "WAVE";

    WavHeader() : Chunk("RIFF") {
      addField(format);
    }
  };

  struct WavFmt : public Chunk {
    u2b audioFormat = 3; // Uncompressed floats
    u2b numChannels = _channels;
    u4b sampleRate = 44100;
    u2b bitsPerSample = sizeof(sample_type) * 8;
    u4b byteRate = sampleRate * numChannels * bitsPerSample/8;
    u2b blockAlign = numChannels * bitsPerSample/8;
    u4b cbSize = 0;
    WavFmt() : Chunk("fmt ") {
      addField(audioFormat);
      addField(numChannels);
      addField(sampleRate);
      addField(byteRate);
      addField(blockAlign);
      addField(bitsPerSample);
      addField(cbSize);
    };
  };

  struct AudioChunk : public Chunk {
    AudioChunk() : Chunk("data") {};
  };

  WavHeader wavHeader;
  WavFmt wavFmt;
  AudioChunk audioChunk;

public:

  struct AudioFrame {

    const static uint size = sizeof(sample_type) * _channels;
    sample_type data[_channels];

    AudioFrame() : data {0} {}
    AudioFrame(sample_type init) :
      data {init} {}

    sample_type &operator[](uint i) {
      return data[i];
    }

    const sample_type &operator[](uint i) const {
      return data[i];
    }

  };

  const static uint channels = _channels;

  BasicSndFile() : File() {
    setupChunks();
    wavHeader.subChunk(wavFmt);
    wavHeader.subChunk(audioChunk);
    addChunk(wavHeader);
  }

  BasicSndFile(std::string path) : BasicSndFile() {
    open(path);
  }

  virtual ~BasicSndFile() {}

  BasicSndFile(BasicSndFile&) = delete;
  BasicSndFile(BasicSndFile&&) = delete;

  void seek(uint pos) {
    fseek(audioChunk.offset + 8 + pos * AudioFrame::size);
  }

  std::size_t position() {
    return (rpos() - audioChunk.offset - 8) / AudioFrame::size;
  }

  std::size_t size() const {
    return audioChunk.size / AudioFrame::size;
  }

  uint read(AudioFrame* data, uint nframes) {
    return read(reinterpret_cast<float *>(data), nframes);
  }

  uint write(AudioFrame* data, uint nframes) {
    return write(reinterpret_cast<float *>(data), nframes);
  }

  uint read(float* data, uint nframes) {
    uint framesRead = 0;
    try {
      for (uint i = 0; i < nframes; ++i) {
        readBytes(data + i * channels, channels);
        ++framesRead;
      }
    } catch (ReadException e) {
      if (e.type != e.END_OF_FILE) LOGE << e.message;
    }
    fseek(rpos());
    return framesRead;
  }

  uint write(float* data, uint nframes) {
    uint framesWritten = 0;
    try {
      for (uint i = 0; i < nframes; ++i) {
        writeBytes(data + i * channels, channels);
        ++framesWritten;
      }
    } catch (ReadException e) {
      if (e.type != e.END_OF_FILE) LOGE << e.message;
    }
    uint newSize = wpos() - audioChunk.offset - 8;
    if (newSize > audioChunk.size) {
      wavHeader.size += newSize - audioChunk.size;
      audioChunk.size = newSize;
    }
    fseek(wpos());
    return framesWritten;

  }

  uint &samplerate = wavFmt.sampleRate;

  void open(std::string path) override {
    File::open(path);
    seek(0);
  }

protected:

  virtual void setupChunks() {}

};

template<uint channels>
using SndFile = BasicSndFile<float, channels>;


}
