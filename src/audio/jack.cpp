#include <cstdlib>
#include <string>
#include <vector>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <plog/Log.h>

#include "../globals.h"
#include "jack.h"
#include "../events.h"

using byte = unsigned char;

static void jackError(const char* s) {
  LOGE << "JACK: " << s;
}

static void jackLogInfo(const char* s) {
  LOGI << "JACK: " << s;
}

void JackAudio::init() {
  client = jack_client_open(CLIENT_NAME.c_str(), JackNullOption, &jackStatus);

  if (!jackStatus & JackServerStarted) {
    LOGF << "Failed to start jack server";
    GLOB.exit();
    return;
  }

  LOGI << "Jack server started";
  LOGD << "Jack client status: " << jackStatus;

  jack_set_process_callback(
    client,
    [](jack_nframes_t nframes, void *arg) {
      ((JackAudio*)arg)->process(nframes);
      return 0;
    }, this);

  jack_set_sample_rate_callback(
    client,
    [](jack_nframes_t nframes, void *arg) {
      ((JackAudio*)arg)->samplerateCallback(nframes);
      return 0;
    }, this);

  jack_set_buffer_size_callback(
    client,
    [](jack_nframes_t nframes, void *arg) {
      ((JackAudio*)arg)->buffersizeCallback(nframes);
      return 0;
    }, this);

  jack_set_error_function(jackError);
  jack_set_info_function(jackLogInfo);

  jack_on_shutdown(client,
   [] (void *arg) {
     ((JackAudio*) arg)->exit();
   }, this);

  bufferSize = jack_get_buffer_size(client);

  if (jack_activate(client)) {
    LOGF << "Cannot activate JACK client";
    GLOB.exit();
    return;
  }

  setupPorts();

  LOGI << "Initialized JackAudio";
}

void JackAudio::startProcess() {
  processing = true;
}

void JackAudio::exit() {
  LOGI << "Closing Jack client";
  jack_client_close(client);
  GLOB.exit();
}

void JackAudio::samplerateCallback(uint srate) {
  LOGI << fmt::format("Jack changed the sample rate to {}", srate);
  GLOB.samplerate = srate;
  GLOB.events.samplerateChanged(srate);
}

void JackAudio::buffersizeCallback(uint buffsize) {
  LOGI << fmt::format("Jack changed the buffer size to {}", buffsize);
  bufferSize = buffsize;
  GLOB.events.bufferSizeChanged(buffsize);
}

void JackAudio::setupPorts() {

  // Audio ports
  ports.input = jack_port_register(
    client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

  if (ports.input == NULL) {
    LOGF << "Couldn't register input port";
    GLOB.exit();
    return;
  }

  ports.outL = jack_port_register(
    client, "outLeft", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  ports.outR = jack_port_register(
    client, "outRight", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  auto inputs  = findPorts(JackPortIsPhysical | JackPortIsOutput);
  auto outputs = findPorts(JackPortIsPhysical | JackPortIsInput);

  if (inputs.empty()) {
    LOGF << "Couldn't find physical input port";
    GLOB.exit();
    return;
  }
  if (outputs.empty()) {
    LOGF << "Couldn't find physical output ports";
    GLOB.exit();
    return;
  }

  bool s;

  s = connectPorts(jack_port_name(ports.input), inputs[0]);
  if (!s) {GLOB.exit(); return;}

  s = connectPorts(outputs[0 % outputs.size()], jack_port_name(ports.outL));
  if (!s) {GLOB.exit(); return;}

  s = connectPorts(outputs[1 % outputs.size()], jack_port_name(ports.outR));
  if (!s) {GLOB.exit(); return;}


  // Midi ports
  ports.midiIn = jack_port_register(
    client, "midiIn", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);

  if (ports.midiIn == NULL) {
    LOGF << "Couldn't register midi_in port";
    GLOB.exit();
    return;
  }

  ports.midiOut = jack_port_register(
    client, "midiOut", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);

  auto midiIn  = findPorts(JackPortIsPhysical | JackPortIsOutput, PortType::MIDI);
  auto midiOut = findPorts(JackPortIsPhysical | JackPortIsInput, PortType::MIDI);

  if (midiIn.empty()) {
    LOGE << "Couldn't find physical midi input port";
    return;
  }
  if (midiOut.empty()) {
    LOGE << "Couldn't find physical midi output port";
    return;
  }

  s = connectPorts(jack_port_name(ports.midiIn), midiIn[0]);
  if (!s) {
    LOGE << "Couldn't connect midi input";
  }

  s = connectPorts(midiOut[0], jack_port_name(ports.midiOut));
  if (!s) {
    LOGE << "Couldn't connect midi output";
  }
}

std::vector<std::string> JackAudio::findPorts(int criteria, PortType type) {
  const char **ports = jack_get_ports(
    client,
    NULL,
    (type == PortType::AUDIO) ? "audio" : "midi",
    criteria);
  std::vector<std::string> ret;
  if (ports == nullptr) return ret;
  for (int i = 0; ports[i] != nullptr; i++) {
    ret.push_back(ports[i]);
  }
  return ret;
};

// Helper function for connections
bool JackAudio::connectPorts(std::string src, std::string dest) {
  return not jack_connect(client, dest.c_str(), src.c_str());
}

void shutdown(void *arg) {
  LOGI << "JACK shut down, exiting";
}

void JackAudio::process(uint nframes) {
  if ( not (processing && GLOB.running())) return;
  if ( nframes > bufferSize) {
    LOGE << "Jack requested more frames than expected";
    return;
  }

  float *outLData = (float *) jack_port_get_buffer(ports.outL, nframes);
  float *outRData = (float *) jack_port_get_buffer(ports.outR, nframes);
  float *inData = (float *) jack_port_get_buffer(ports.input, nframes);

  GLOB.audioData.outL.clear();
  GLOB.audioData.outR.clear();
  GLOB.audioData.input.clear();
  GLOB.audioData.proc.clear();

  for (uint i = 0; i < nframes; i++) {
    GLOB.audioData.input[i] = inData[i];
  }

  // Midi events
  {
    GLOB.midiEvents.clear();

    // Get new ones
    void *midiBuf = jack_port_get_buffer(ports.midiIn, nframes);
    uint nevents = jack_midi_get_event_count(midiBuf);

    jack_midi_event_t event;
    for (uint i = 0; i < nevents; i++) {
      jack_midi_event_get(&event, midiBuf, i);
      MidiEvent mEvent;

      mEvent.channel = event.buffer[0] & 0b00001111;
      mEvent.data = event.buffer + 1;
      mEvent.time = event.time;

      byte type = event.buffer[0] >> 4;
      switch (type) {
      case MidiEvent::NOTE_OFF:
        // LOGD << "NOTE_OFF";
        mEvent.type = MidiEvent::NOTE_OFF;
        GLOB.midiEvents.emplace_back(new NoteOffEvent(mEvent));
        break;
      case MidiEvent::NOTE_ON:
        // LOGD << "NOTE_ON";
        mEvent.type = MidiEvent::NOTE_ON;
        GLOB.midiEvents.emplace_back(new NoteOnEvent(mEvent));
        break;
      case MidiEvent::CONTROL_CHANGE:
        // LOGD << "CONTROL_CHANGE";
        mEvent.type = MidiEvent::CONTROL_CHANGE;
        GLOB.midiEvents.emplace_back(new ControlChangeEvent(mEvent));
        break;
      }
    }
  }

  GLOB.tapedeck.preProcess(nframes);
  GLOB.synth.process(nframes);
  GLOB.drums.process(nframes);
  GLOB.effect.process(nframes);
  GLOB.tapedeck.postProcess(nframes);
  GLOB.mixer.process(nframes);
  GLOB.metronome.process(nframes);

  for (uint i = 0; i < nframes; i++) {
    outLData[i] = GLOB.audioData.outL[i];
    outRData[i] = GLOB.audioData.outR[i];
  }

}
