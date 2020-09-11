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
 * 5. The protocol must be backward-compatible with the existing Tympan Remote app.
 * 
 * The grammar for the protocol is represented by the following EBNF grammar:
 * 
 * channel_identifier ::= ? integer between 0 and 99 inclusive ?
 * knob_identifier    ::= "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L" | "M"
 *                      | "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z"
 * end_of_message     ::= ";"
 * basic_command      ::= ? any character except semicolon ?
 * basic_mode         ::= "\;"
 * extended_mode      ::= "/"
 * help_command       ::= "?" , end_of_message
 * get_layout_command ::= "#" , end_of_message
 * run_command        ::= "!" , basic_command , end_of_message
 * activate_command   ::= "^" , [channel_identifier] , knob_identifier , end_of_message
 * query_all_command  ::= "&&" , end_of_message
 * query_command      ::= "&" , [channel_identifier] , knob_identifier , end_of_message
 * increment_command  ::= "+" , [channel_identifier] , knob_identifier , end_of_message
 * decrement_command  ::= "-" , [channel_identifier] , knob_identifier , end_of_message
 * set_command        ::= "*" , [channel_identifier] , knob_identifier , ? integer between 0 and 99 inclusive ? , end_of_message
 * 
 * Several commands are reserved by the protocol:
 *  J - execute get_layout command
 *  h - execute help command
 *  \ - switch to basic mode
 *  / - switch to extended mode
 * 
 * Responses intended exclusively for human consumption will be prefixed with "Msg: ".
 *
 * Responses will generally take the form of <identifier>=<value>. Any command that does
 * not have an inherent value will return a special ACK=1 response (for success) or
 * ACK=0 response (for failure).
 * 
 * Responses are newline delimited rather than semicolon delimited.
 */

#define PRINT_MESSAGES_FOR_HUMANS    false

#define TYMPAN_ESM_BASIC_MODE_COMMAND '\\'
#define TYMPAN_ESM_HELP_COMMAND       '?'
#define TYMPAN_ESM_GET_LAYOUT_COMMAND '#'
#define TYMPAN_ESM_RUN_COMMAND        '!'
#define TYMPAN_ESM_ACTIVATE_COMMAND   '^'
#define TYMPAN_ESM_QUERY_COMMAND      '&'
#define TYMPAN_ESM_INCREMENT_COMMAND  '+'
#define TYMPAN_ESM_DECREMENT_COMMAND  '-'
#define TYMPAN_ESM_SET_COMMAND        '*'

#include <ctype.h>
#include <Tympan_Library.h>

extern Tympan myTympan;               //defined in main *.ino file
extern bool enable_printCPUandMemory; //defined in main *.ino file


enum MODE {
  Basic,
  Extended
};

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

class ExtendedSerialManager {
  public:
    ExtendedSerialManager(
      CONFIGURABLE knobs[],
      int channelCount,
      int knobCount,
      void (*apply)(void),
      void (*activate)(int channel, int knob),
      bool (*run)(char cmd)
    );

    void processStream(Stream *stream);
    void processExtendedCommand(char *cmd);

  protected:
    void handleHelpCommand(void);
    void handleGetLayoutCommand(void);
    void handleRunCommand(char *options);
    void handleActivateCommand(char *options);
    void handleQueryCommand(char *options);
    void handleIncrementCommand(char *options);
    void handleDecrementCommand(char *options);
    void handleSetCommand(char *options);
      
  private:
    MODE mode = Basic;
    char buffer[8];
    CONFIGURABLE *knobs;
    int channelCount;
    int knobCount;
    void (*apply)(void);
    void (*activate)(int channel, int knob);
    bool (*run)(char cmd);

    CMD_OPTIONS parseOptions(char *options);
    CONFIGURABLE *getKnob(int channel, int knob);
    CONFIGURABLE *getKnob(CMD_OPTIONS opts);
    char getKnobIdentifier(int knob);
    void printValue(CONFIGURABLE *knob, const char *verb, const float oldVal);
    void printValue(CONFIGURABLE *knob);
    void ackIfExtended();
    void ackIfExtended(bool success);
};

ExtendedSerialManager::ExtendedSerialManager(
  CONFIGURABLE knobs[],
  int channelCount,
  int knobCount,
  void (*apply)(void),
  void (*activate)(int channel, int knob),
  bool (*run)(char cmd)
) {
  this->knobs = knobs;
  this->channelCount = channelCount;
  this->knobCount = knobCount;
  this->apply = apply;
  this->activate = activate;
  this->run = run;
};

void ExtendedSerialManager::processStream(Stream *stream) {
  int count;
  while (stream->available()) {
    if (mode == Extended) {
      count = stream->readBytesUntil(';', (char *)&buffer, 8);
      if (count && (count < 8 || buffer[7] == ';')) {
        processExtendedCommand(buffer);
      } else {
        #if (PRINT_MESSAGES_FOR_HUMANS)
          myTympan.println("Msg: unable to parse message");
        #endif
        myTympan.println("ACK=0");
      }
    } else {
      char c = stream->read();
      handleRunCommand(&c);
    }
  }
}

void ExtendedSerialManager::processExtendedCommand(char *cmd) {
  switch (cmd[0]) {
    case TYMPAN_ESM_BASIC_MODE_COMMAND: mode = Basic; break;
    case TYMPAN_ESM_HELP_COMMAND: handleHelpCommand(); break;
    case TYMPAN_ESM_GET_LAYOUT_COMMAND: handleGetLayoutCommand(); break;
    case TYMPAN_ESM_RUN_COMMAND: handleRunCommand(&cmd[1]); break;
    case TYMPAN_ESM_ACTIVATE_COMMAND: handleActivateCommand(&cmd[1]); break;
    case TYMPAN_ESM_QUERY_COMMAND: handleQueryCommand(&cmd[1]); break;
    case TYMPAN_ESM_INCREMENT_COMMAND: handleIncrementCommand(&cmd[1]); break;
    case TYMPAN_ESM_DECREMENT_COMMAND: handleDecrementCommand(&cmd[1]); break;
    case TYMPAN_ESM_SET_COMMAND: handleSetCommand(&cmd[1]); break;
    default:
      #if (PRINT_MESSAGES_FOR_HUMANS)
        myTympan.println("Unable to parse command");
      #endif
      myTympan.println("ACK=0");
      return;
  }
}

void ExtendedSerialManager::handleHelpCommand(void) {
  myTympan.println("Msg: Extended Serial Manager Help.");
  myTympan.printf("Msg: Channels: %i\n", channelCount);
  myTympan.println("Msg: Commands:");
  myTympan.println("Msg:   \\; - switch to basic (legacy) mode");
  myTympan.println("Msg:   / - switch to extended mode (note the lack of a semicolon)");
  myTympan.println("Msg:   ?; - print this help");
  myTympan.println("Msg:   #; - print layout JSON");
  myTympan.println("Msg:   !<command>; - run the specified 1-character command (equivalent to basic-mode commands)");
  myTympan.println("Msg:   ^[channel]<knob>; - activate specified knob for optionally specified channel");
  myTympan.println("Msg:   &[channel]<knob>; - query current value for specified knob of optionally specified channel");
  myTympan.println("Msg:   +[channel]<knob>; - increment  current value for specified knob of optionally specified channel");
  myTympan.println("Msg:   -[channel]<knob>; - decrement current value for specified knob of optionally specified channel");
  myTympan.println("Msg:   *[channel]<knob><value>; - set current value for specified knob of optionally specified channel as percentage of range");
  myTympan.println("Msg: Knobs:");
  for (int ii = 0; ii < knobCount; ii++) {
    myTympan.printf("Msg:   %c - %s (%f%s-%f%s)\n", getKnobIdentifier(ii), knobs[ii].name, knobs[ii].min, knobs[ii].unit, knobs[ii].max, knobs[ii].unit);
  }
}

void ExtendedSerialManager::handleGetLayoutCommand(void) {

}

void ExtendedSerialManager::handleRunCommand(char *options) {
  switch (options[0]) {
    case '/': mode = Extended; myTympan.println("ACK=1"); break;
    case '\\': mode = Basic; myTympan.println("ACK=1"); break;
    case 'h': handleHelpCommand(); ackIfExtended(); break;
    case 'J': handleGetLayoutCommand(); ackIfExtended(); break;
    default: ackIfExtended(run(options[0]));
  }
}

void ExtendedSerialManager::handleActivateCommand(char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  #if (PRINT_MESSAGES_FOR_HUMANS)
    myTympan.printf(
        "Msg: Activating %s (%c) on channel %i\n",
        getKnob(opts)->name,
        getKnobIdentifier(opts.knob),
        opts.channel
    );
  #endif
  activate(opts.channel, opts.knob);
}

void ExtendedSerialManager::handleQueryCommand(char *options) {
  CONFIGURABLE *knob;
  if (options[0] == '&') {
    #if (PRINT_MESSAGES_FOR_HUMANS)
      myTympan.println("Msg: Printing all values");
    #endif
    for (int ii = 0; ii < channelCount; ii++) {
      #if (PRINT_MESSAGES_FOR_HUMANS)
        myTympan.printf("Msg:   Channel %i\n", ii);
      #endif
      for (int jj = 0; jj < knobCount; jj++) {
        knob = getKnob(ii, jj);
        #if (PRINT_MESSAGES_FOR_HUMANS)
          myTympan.printf(
              "Msg:     %s (%c) = %f%s\n",
              knob->name,
              getKnobIdentifier(jj),
              *knob->value,
              knob->unit
          );
        #endif
        printValue(knob);
      }
    }
  } else {
    CMD_OPTIONS opts = parseOptions(options);
    printValue(getKnob(opts));
  }
}

void ExtendedSerialManager::handleIncrementCommand(char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  CONFIGURABLE *knob = getKnob(opts);
  float oldVal = *knob->value;
  float newVal = oldVal + (knob->max - knob->min) * 0.05f;
  *knob->value = newVal > knob->max ? knob->max : newVal;
  printValue(knob, "Incrementing", oldVal);
  apply();
}

void ExtendedSerialManager::handleDecrementCommand(char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  CONFIGURABLE *knob = getKnob(opts);
  float oldVal = *knob->value;
  float newVal = oldVal - (knob->max - knob->min) * 0.05f;
  *knob->value = newVal < knob->min ? knob->min : newVal;
  printValue(knob, "Decrementing", oldVal);
  apply();
}

void ExtendedSerialManager::handleSetCommand(char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  CONFIGURABLE *knob = getKnob(opts);
  float oldVal = *knob->value;
  float newVal = knob->min + (knob->max - knob->min) * (opts.value / 100.0f);
  *knob->value = newVal < knob->min ? knob->min : newVal > knob->max ? knob->max : newVal;
  printValue(knob, "Setting", oldVal);
  apply();
}

CMD_OPTIONS ExtendedSerialManager::parseOptions(char *options) {
  CMD_OPTIONS parsed = { 0, 0, 0 };
  char *ptr = options;
  while (isDigit(*ptr)) {
    // This is probably terrible form
    parsed.channel = parsed.channel * 10 + (*ptr - '0');
    ptr++;
  }
  // Just in case, let's support a lower-case knob identifier
  parsed.knob = (*ptr & 0x20) ? *ptr - 'a' : *ptr - 'A';
  while (isDigit(*ptr)) {
    parsed.value = parsed.value * 10 + (*ptr - '0');
    ptr++;
  }
  return parsed;
}

inline CONFIGURABLE *ExtendedSerialManager::getKnob(int channel, int knob) {
  return &knobs[channel * knobCount + knob];
}

inline CONFIGURABLE *ExtendedSerialManager::getKnob(CMD_OPTIONS opts) {
  return &knobs[opts.channel * knobCount + opts.knob];
}

inline char ExtendedSerialManager::getKnobIdentifier(int knob) {
  return 'A' + knob;
}

inline void ExtendedSerialManager::printValue(CONFIGURABLE *knob) {
  int channel = (knob - knobs) / sizeof(CONFIGURABLE);
  char knobIdentifier = (knob - knobs) % sizeof(CONFIGURABLE);
  myTympan.print(knobIdentifier);
  myTympan.print(channel);
  myTympan.print("=");
  myTympan.print(*knob->value);
  myTympan.print("\n");
}

inline void ExtendedSerialManager::printValue(CONFIGURABLE *knob, const char *verb, const float oldVal) {
  int channel = (knob - knobs) / sizeof(CONFIGURABLE);
  char knobIdentifier = (knob - knobs) % sizeof(CONFIGURABLE);
  #if (PRINT_MESSAGES_FOR_HUMANS)
    myTympan.printf(
        "Msg: %s %s (%c) on channel %i from %f%s to %f%s (clamped)\n",
        verb,
        knob->name,
        knobIdentifier,
        channel,
        oldVal,
        knob->unit,
        *knob->value,
        knob->unit
    );
  #endif
  myTympan.print(knobIdentifier);
  myTympan.print(channel);
  myTympan.print("=");
  myTympan.print(*knob->value);
  myTympan.print("\n");
}

void ExtendedSerialManager::ackIfExtended() {
  if (mode == Extended) myTympan.println("ACK=1");
}

void ExtendedSerialManager::ackIfExtended(bool success) {
  if (mode == Extended) {
    if (success) myTympan.println("ACK=1");
    else myTympan.println("ACK=0");
  }
}

#endif
