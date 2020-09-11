#ifndef _ExtendedSerialManager_h
#define _ExtendedSerialManager_h

/*
 * Our extended serial protocol is intended to solve for the following use cases:
 * 1. The user wishes to interrogate the current value of a knob.
 * 2. The user wishes to directly set the current value of a knob within the knob's configured minimum/maximum.
 * 3. The user wishes to increment or decrement the current value of a knob.
 * 4. The user wishes to be presented with a series of device-defined screens representing a GUI to manipulate all knobs.
 * 
 * Our key design concepts are:
 * 1. The protocol must be human-readable.
 * 2. The protocol must be reasonably terse.
 * 3. Implementation details on the Tympan side should be limited to this class as much as possible.
 * 4. No command sent to the Tympan can be more than 8 characters long.
 * 
 * The grammar for the protocol is represented by the following EBNF grammar:
 * 
 * channel_identifier ::= ? integer between 0 and 99 inclusive ?
 * knob_identifier    ::= "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L" | "M"
 *                      | "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z"
 * end_of_message     ::= ";"
 * help_command       ::= "?" , end_of_message
 * get_layout_command ::= "#" , end_of_message
 * activate_command   ::= "!" , [channel_identifier] , knob_identifier , end_of_message
 * query_all_command  ::= "&&" , end_of_message
 * query_command      ::= "&" , [channel_identifier] , knob_identifier , end_of_message
 * increment_command  ::= "+" , [channel_identifier] , knob_identifier , end_of_message
 * decrement_command  ::= "-" , [channel_identifier] , knob_identifier , end_of_message
 * set_command        ::= "*" , [channel_identifier] , knob_identifier , ? integer between 0 and 99 inclusive ? , end_of_message
 */

#define TYMPAN_ESM_HELP_COMMAND       '?'
#define TYMPAN_ESM_GET_LAYOUT_COMMAND '#'
#define TYMPAN_ESM_ACTIVATE_COMMAND   '!'
#define TYMPAN_ESM_QUERY_COMMAND      '&'
#define TYMPAN_ESM_INCREMENT_COMMAND  '+'
#define TYMPAN_ESM_DECREMENT_COMMAND  '-'
#define TYMPAN_ESM_SET_COMMAND        '*'

#include <ctype.h>
#include <Tympan_Library.h>

extern Tympan myTympan;               //defined in main *.ino file
extern bool enable_printCPUandMemory; //defined in main *.ino file

typedef struct {
  const char *name;
  float *value;
  const char *unit;
  float min;
  float max;
} CONFIGURABLE;

typedef struct {
  int channel;
  int knob;
  int value;
} CMD_OPTIONS;

//functions in the main sketch that I want to call from here
extern void incrementKnobGain(float);
extern void printGainSettings(void);
extern void togglePrintAveSignalLevels(bool);
// extern void incrementDSLConfiguration(void);

//add in the algorithm whose gains we wish to set via this SerialManager...change this if your gain algorithms class changes names!
#include "AudioEffectCompWDRC_F32.h"    //change this if you change the name of the algorithm's source code filename
typedef AudioEffectCompWDRC_F32 GainAlgorithm_t; //change this if you change the algorithm's class name

//now, define the Serial Manager class
class ExtendedSerialManager {
  public:
    ExtendedSerialManager(CONFIGURABLE knobs[], int channelCount, int knobCount, void (*apply)(void), void (*activate)(int channel, int knob));
    void processStream(Stream *stream);
    void processCommand(char *cmd);

  protected:
    void handleHelpCommand(void);
    void handleGetLayoutCommand(void);
    void handleActivateCommand(char *options);
    void handleQueryCommand(char *options);
    void handleIncrementCommand(char *options);
    void handleDecrementCommand(char *options);
    void handleSetCommand(char *options);
      
  private:
    char buffer[8];
    CONFIGURABLE *knobs;
    int channelCount;
    int knobCount;
    void (*apply)(void);
    void (*activate)(int channel, int knob);

    CMD_OPTIONS parseOptions(char *options);
    CONFIGURABLE getKnob(int channel, int knob);
    CONFIGURABLE getKnob(CMD_OPTIONS opts);
};

ExtendedSerialManager::ExtendedSerialManager(CONFIGURABLE knobs[], int channelCount, int knobCount, void(*apply)(void), void(*activate)(int channel, int knob)) {
  this->knobs = knobs;
  this->channelCount = channelCount;
  this->knobCount = knobCount;
  this->apply = apply;
  this->activate = activate;
}

void ExtendedSerialManager::processStream(Stream *stream) {
  while (stream->available()) {
    int count = stream->readBytesUntil(';', (char *)&buffer, 8);
    if (count && (count < 8 || buffer[7] == ';')) {
      processCommand(buffer);
    } else {
      myTympan.println("Error: unable to parse message");
    }
  }
}

void ExtendedSerialManager::processCommand(char *cmd) {
  switch (cmd[0]) {
    case TYMPAN_ESM_HELP_COMMAND: handleHelpCommand(); break;
    case TYMPAN_ESM_GET_LAYOUT_COMMAND: handleGetLayoutCommand(); break;
    case TYMPAN_ESM_ACTIVATE_COMMAND: handleActivateCommand(&cmd[1]); break;
    case TYMPAN_ESM_QUERY_COMMAND: handleQueryCommand(&cmd[1]); break;
    case TYMPAN_ESM_INCREMENT_COMMAND: handleIncrementCommand(&cmd[1]); break;
    case TYMPAN_ESM_DECREMENT_COMMAND: handleDecrementCommand(&cmd[1]); break;
    case TYMPAN_ESM_SET_COMMAND: handleSetCommand(&cmd[1]); break;
    default:
      myTympan.println("Unable to parse command");
  }
}

void ExtendedSerialManager::handleHelpCommand(void) {
  myTympan.println("Msg: Extended Serial Manager Help.");
  myTympan.printf("Msg: Channels: %i\n", channelCount);
  myTympan.println("Msg: Commands:");
  myTympan.println("Msg:   ?; - print this help");
  myTympan.println("Msg:   #; - print layout JSON");
  myTympan.println("Msg:   ![channel]<knob>; - activate specified knob for optionally specified channel");
  myTympan.println("Msg:   &[channel]<knob>; - query current value for specified knob of optionally specified channel");
  myTympan.println("Msg:   +[channel]<knob>; - increment  current value for specified knob of optionally specified channel");
  myTympan.println("Msg:   -[channel]<knob>; - decrement current value for specified knob of optionally specified channel");
  myTympan.println("Msg:   *[channel]<knob><value>; - set current value for specified knob of optionally specified channel as percentage of range");
  myTympan.println("Msg: Knobs:");
  for (int ii = 0; ii < knobCount; ii++) {
    myTympan.printf("Msg:   %c - %s (%f%s-%f%s)\n", 'A' + ii, knobs[ii].name, knobs[ii].min, knobs[ii].unit, knobs[ii].max, knobs[ii].unit);
  }
}

void ExtendedSerialManager::handleGetLayoutCommand(void) {

}

void ExtendedSerialManager::handleActivateCommand(char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  myTympan.printf(
      "Msg: Activating %s (%c) on channel %i\n",
      getKnob(opts).name,
      'A' + opts.knob,
      opts.channel
  );
  activate(opts.channel, opts.knob);
}

void ExtendedSerialManager::handleQueryCommand(char *options) {
  if (options[0] == '&') {
    myTympan.println("Msg: Printing all values");
    for (int ii = 0; ii < channelCount; ii++) {
      myTympan.printf("Msg:   Channel %i\n", ii);
      for (int jj = 0; jj < knobCount; jj++) {
        myTympan.printf(
            "Msg:     %s (%c) = %f%s\n",
            getKnob(ii, jj).name,
            'A' + jj,
            *getKnob(ii, jj).value,
            getKnob(ii, jj).unit
        );
      }
    }
  } else {
    CMD_OPTIONS opts = parseOptions(options);
    myTympan.printf(
        "Msg: %s (%c) on channel %i = %f%s\n",
        getKnob(opts).name,
        'A' + opts.knob,
        opts.channel,
        *getKnob(opts).value,
        getKnob(opts).unit
    );
  }
}

void ExtendedSerialManager::handleIncrementCommand(char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  float oldVal = *getKnob(opts).value;
  float newVal = oldVal + (getKnob(opts).max - getKnob(opts).min) * 0.05f;
  *getKnob(opts).value = newVal > getKnob(opts).max ? getKnob(opts).max : newVal;
  myTympan.printf(
      "Msg: Incrementing %s (%c) on channel %i from %f%s to %f%s\n",
      getKnob(opts).name,
      'A' + opts.knob,
      opts.channel,
      oldVal,
      getKnob(opts).unit,
      newVal,
      getKnob(opts).unit
  );
  apply();
}

void ExtendedSerialManager::handleDecrementCommand(char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  float oldVal = *getKnob(opts).value;
  float newVal = oldVal - (getKnob(opts).max - getKnob(opts).min) * 0.05f;
  *getKnob(opts).value = newVal < getKnob(opts).min ? getKnob(opts).min : newVal;
  myTympan.printf(
      "Msg: Incrementing %s (%c) on channel %i from %f%s to %f%s\n",
      getKnob(opts).name,
      'A' + opts.knob,
      opts.channel,
      oldVal,
      getKnob(opts).unit,
      newVal,
      getKnob(opts).unit
  );
  apply();
}

void ExtendedSerialManager::handleSetCommand(char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  float oldVal = *getKnob(opts).value;
  float newVal = getKnob(opts).min + (getKnob(opts).max - getKnob(opts).min) * (opts.value / 100.0f);
  *getKnob(opts).value = newVal < getKnob(opts).min ? getKnob(opts).min : newVal > getKnob(opts).max ? getKnob(opts).max : newVal;
  myTympan.printf(
      "Msg: Setting %s (%c) on channel %i from %f%s to %f%s (clamped)\n",
      getKnob(opts).name,
      'A' + opts.knob,
      opts.channel,
      oldVal,
      getKnob(opts).unit,
      newVal,
      getKnob(opts).unit
  );
  apply();
}

CMD_OPTIONS ExtendedSerialManager::parseOptions(char *options) {
  CMD_OPTIONS parsed = { 0, 0, 0 };
  char *ptr = options;
  while (isDigit(*ptr)) {
    parsed.channel = parsed.channel * 10 + (*ptr - '0');
    ptr++;
  }
  parsed.knob = (*ptr & 0x20) ? *ptr - 'a' : *ptr - 'A';
  while (isDigit(*ptr)) {
    parsed.value = parsed.value * 10 + (*ptr - '0');
    ptr++;
  }
  return parsed;
}

inline CONFIGURABLE ExtendedSerialManager::getKnob(int channel, int knob) {
  return knobs[channel * knobCount + knob];
}

inline CONFIGURABLE ExtendedSerialManager::getKnob(CMD_OPTIONS opts) {
  return knobs[opts.channel * knobCount + opts.knob];
}

#endif
