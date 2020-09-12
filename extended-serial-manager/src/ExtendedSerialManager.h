#ifndef _ExtendedSerialManager_h
#define _ExtendedSerialManager_h

/*
 *
 * PROTOCOL
 * 
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
 * 4. No command sent to the Tympan can be more than 256 characters long, and most should be under 8.
 * 5. The protocol must be backward-compatible with the existing Tympan Remote app.
 * 
 * The grammar for the protocol is represented by the following EBNF grammar:
 * 
 * channel_identifier ::= ? integer between 0 and 99 inclusive ?
 * knob_identifier    ::= "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L" | "M"
 *                      | "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z"
 * end_of_message     ::= ";"
 * basic_command      ::= ? any ASCII (7-bit) character except semicolon ?
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
 * set_command        ::= "*" , [channel_identifier] , knob_identifier
 *                      , ? integer between 0 and 99 inclusive ? , end_of_message
 * apply_command      ::= "=" , (channel_identifier | knob_identifier) , "="
 *                      , ? float value ? , {"," , ? float value ?} , end_of_message
 * 
 * Several commands are reserved by the protocol:
 *  J - execute get_layout command
 *  h - execute help command
 *  \ - switch to basic mode
 *  / - switch to extended mode
 * 
 * Whitespace is probably not a good choice for commands, even if it is technically permissible.
 * 
 * Responses intended exclusively for human consumption will be prefixed with "Msg: ".
 *
 * Responses will generally take the form of <identifier>=<value>. Any command that does
 * not have an inherent value will return a special ACK=1 response (for success) or
 * ACK=0 response (for failure).
 * 
 * Responses are newline delimited rather than semicolon delimited.
 * 
 */

/*
 *
 * API
 * 
 * The extended serial manager is configured with a set of knob definitions and command definitions
 * (plus counts, and two mandatory callbacks). See the inline comments below.
 * 
 */

#define PRINT_MESSAGES_FOR_HUMANS    true

#define TYMPAN_ESM_BASIC_MODE_COMMAND '\\'
#define TYMPAN_ESM_HELP_COMMAND       '?'
#define TYMPAN_ESM_GET_LAYOUT_COMMAND '#'
#define TYMPAN_ESM_RUN_COMMAND        '!'
#define TYMPAN_ESM_ACTIVATE_COMMAND   '^'
#define TYMPAN_ESM_QUERY_COMMAND      '&'
#define TYMPAN_ESM_INCREMENT_COMMAND  '+'
#define TYMPAN_ESM_DECREMENT_COMMAND  '-'
#define TYMPAN_ESM_SET_COMMAND        '*'
#define TYMPAN_ESM_APPLY_COMMAND      '='
#define TYMPAN_ESM_END_OF_MESSAGE     ';'

#include <ctype.h>
#include <Tympan_Library.h>

extern Tympan myTympan;
extern bool enable_printCPUandMemory;


enum MODE {
  Basic,
  Extended
};

typedef struct {
  const char *name; // name of the knob for help purposes (e.g. "tk")
  float *value;     // pointer to where the value should be stored
  const char *unit; // unit in which the knob is defined for help purposes (e.g. "ms")
  float min;        // minimum value for the knob
  float max;        // maximum value for the knob
} CONFIGURABLE;

typedef struct {
  const char character;     // 7-bit ASCII character which should trigger the command (e.g. 'k')
  const char *name;         // name of the command for help purposes (e.g. "increase gain")
  bool (*execute)(char c);  // function which will execute the command and return
                            //   true if the command executes successfully and false otherwise.
                            //   The callback will be passed the character which triggered the command
} COMMAND;

typedef struct {
  int channel;
  int knob;
  int value;
} CMD_OPTIONS;

class ExtendedSerialManager {
  public:
    ExtendedSerialManager(
      CONFIGURABLE knobs[],                   // pointer to the list of configured knobs
      int channelCount,                       // number of channels
      int knobCount,                          // number of knobs available per channel
      COMMAND commands[],                     // pointer to the list of configured commands
      int commandCount,                       // number of commands
      void (*apply)(void),                    // function that will apply the updated configuration
      void (*activate)(int channel, int knob) // function that will "activate" the specified
                                              //   channel/knob configuration (e.g. for potentiometer control)
    );

    void processByte(char c);
    void processExtendedCommand(char *cmd);

  protected:
    void handleHelpCommand(void);
    void handleGetLayoutCommand(void);
    void handleRunCommand(const char *options);
    void handleActivateCommand(const char *options);
    void handleQueryCommand(const char *options);
    void handleIncrementCommand(const char *options);
    void handleDecrementCommand(const char *options);
    void handleSetCommand(const char *options);
    void handleApplyCommand(const char *options);
      
  private:
    MODE mode = Basic;

    // input buffer
    char buffer[256];
    char *bufferPtr = buffer;

    // knob configuration
    CONFIGURABLE *knobs;
    int channelCount;
    int knobCount;

    // command configuration
    COMMAND *commands;
    bool (*commandLut[128])(char c);
    int commandCount;

    // mandatory helper methods
    void (*apply)(void);
    void (*activate)(int channel, int knob);

    CMD_OPTIONS parseOptions(const char *options);
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
  COMMAND commands[],
  int commandCount,
  void (*apply)(void),
  void (*activate)(int channel, int knob)
) {
  this->knobs = knobs;
  this->channelCount = channelCount;
  this->knobCount = knobCount;
  this->commands = commands;
  this->commandCount = commandCount;
  this->apply = apply;
  this->activate = activate;
  memset(commandLut, 0, sizeof(commandLut));
  for (int ii = 0; ii < commandCount; ii++) {
    commandLut[commands[ii].character & 0x7f] = commands[ii].execute;
  }
};

void ExtendedSerialManager::processByte(char c) {
  if (mode == Basic) {
    handleRunCommand(&c);
  } else {
    if (c == TYMPAN_ESM_END_OF_MESSAGE) {
      *bufferPtr = '\0';
      bufferPtr = buffer;
      processExtendedCommand(buffer);
    } else {
      *bufferPtr++ = c;
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
    case TYMPAN_ESM_APPLY_COMMAND: handleApplyCommand(&cmd[1]); break;
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
  myTympan.println("Msg:   =<channel|knob>=<comma-separated values>; - set all values for a 'slice' (either all knobs for a channel or a particular knob for all channels)");
  myTympan.println("Msg: Knobs:");
  for (int ii = 0; ii < knobCount; ii++) {
    myTympan.printf("Msg:   %c - %s (%f%s-%f%s)\n", getKnobIdentifier(ii), knobs[ii].name, knobs[ii].min, knobs[ii].unit, knobs[ii].max, knobs[ii].unit);
  }
  myTympan.println("Msg: Commands:");
  for (int ii = 0; ii < commandCount; ii++) {
    myTympan.printf("Msg:   %c - %s\n", commands[ii].character, commands[ii].name);
  }
}

void ExtendedSerialManager::handleGetLayoutCommand(void) {

}

void ExtendedSerialManager::handleRunCommand(const char *options) {
  switch (options[0]) {
    case '/': mode = Extended; myTympan.println("ACK=1"); break;
    case '\\': mode = Basic; myTympan.println("ACK=1"); break;
    case 'h': handleHelpCommand(); ackIfExtended(); break;
    case 'J': handleGetLayoutCommand(); ackIfExtended(); break;
    default:
      bool (*execute)(char c) = commandLut[options[0] & 0x7f];
      ackIfExtended(execute ? execute(options[0]) : false);
  }
}

void ExtendedSerialManager::handleActivateCommand(const char *options) {
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

void ExtendedSerialManager::handleQueryCommand(const char *options) {
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

void ExtendedSerialManager::handleIncrementCommand(const char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  CONFIGURABLE *knob = getKnob(opts);
  float oldVal = *knob->value;
  float newVal = oldVal + (knob->max - knob->min) * 0.05f;
  *knob->value = newVal > knob->max ? knob->max : newVal;
  printValue(knob, "Incrementing", oldVal);
  apply();
}

void ExtendedSerialManager::handleDecrementCommand(const char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  CONFIGURABLE *knob = getKnob(opts);
  float oldVal = *knob->value;
  float newVal = oldVal - (knob->max - knob->min) * 0.05f;
  *knob->value = newVal < knob->min ? knob->min : newVal;
  printValue(knob, "Decrementing", oldVal);
  apply();
}

void ExtendedSerialManager::handleSetCommand(const char *options) {
  CMD_OPTIONS opts = parseOptions(options);
  CONFIGURABLE *knob = getKnob(opts);
  float oldVal = *knob->value;
  float newVal = knob->min + (knob->max - knob->min) * (opts.value / 100.0f);
  *knob->value = newVal < knob->min ? knob->min : newVal > knob->max ? knob->max : newVal;
  printValue(knob, "Setting", oldVal);
  apply();
}

void ExtendedSerialManager::handleApplyCommand(const char *options) {
  int channel = 0;
  char knob = 0;
  char *ptr = (char *)options;
  char *nextPtr = (char *)options;
  while (isDigit(*ptr)) {
    // This is probably terrible form
    channel = channel * 10 + (*ptr - '0');
    ptr++;
  }
  knob = *ptr++;
  nextPtr = ptr;
  if (knob == '=') {
    // Channel must have been specified
    CONFIGURABLE *nextKnob = getKnob(channel, 0);
    for (int ii = 0; ii < knobCount; ii++) {
      while (*nextPtr != ',' && *nextPtr != '\0') nextPtr++;
      *nextKnob->value = strtof(ptr, &nextPtr);
      ptr = ++nextPtr;
      nextKnob += 1;
    }
  } else {
    ptr++;
    CONFIGURABLE *nextKnob = getKnob(0, knob & 0x20 ? knob - 'a' : knob - 'A');
    for (int ii = 0; ii < channelCount; ii++) {
      while (*nextPtr != ',' && *nextPtr != '\0') nextPtr++;
      *nextKnob->value = strtof(ptr, &nextPtr);
      ptr = ++nextPtr;
      nextKnob += knobCount;
    }
  }
  handleQueryCommand("&");
  apply();
}

CMD_OPTIONS ExtendedSerialManager::parseOptions(const char *options) {
  CMD_OPTIONS parsed = { 0, 0, 0 };
  char *ptr = (char *)options;
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
  char knobIdentifier = getKnobIdentifier((knob - knobs) % sizeof(CONFIGURABLE));
  myTympan.print(knobIdentifier);
  myTympan.print(channel);
  myTympan.print("=");
  myTympan.print(*knob->value);
  myTympan.print("\n");
}

inline void ExtendedSerialManager::printValue(CONFIGURABLE *knob, const char *verb, const float oldVal) {
  int channel = (knob - knobs) / sizeof(CONFIGURABLE);
  char knobIdentifier = getKnobIdentifier((knob - knobs) % sizeof(CONFIGURABLE));
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
