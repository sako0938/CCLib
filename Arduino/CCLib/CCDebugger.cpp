/**
 *
 * CC-Debugger Protocol Library for Arduino
 * Copyright (c) 2014 Ioannis Charalampidis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <CCDebugger.h>

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
////                CONSTRUCTOR & CONFIGURATORS                  ////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

/**
 * Initialize CC Debugger class
 */
CCDebugger::CCDebugger( int pinRST, int pinDC, int pinDD_I, int pinDD_O ) 
{

  // Keep references
  this->pinRST  = pinRST;
  this->pinDC   = pinDC;
  this->pinDD_I = pinDD_I;
  this->pinDD_O = pinDD_O;

  // Reset LEDS
  this->pinReadLED = 0;
  this->pinWriteLED = 0;

  // Prepare CC Pins
  pinMode(pinDC,        OUTPUT);
  pinMode(pinDD_I,      INPUT);
  pinMode(pinDD_O,      OUTPUT);
  pinMode(pinRST,       OUTPUT);
  digitalWrite(pinDC,   LOW);
  digitalWrite(pinDD_I, LOW); // No pull-up
  digitalWrite(pinDD_O, LOW);
  digitalWrite(pinRST,  LOW);

  // Prepare default direction
  setDDDirection(INPUT);

  // We are active by default
  active = true;
  
};

/**
 * Enable/Configure LEDs
 */
void CCDebugger::setLED( int pinReadLED, int pinWriteLED )
{
  // Prepare read LED
  this->pinReadLED  = pinReadLED;
  if (pinReadLED) {
    pinMode(pinWriteLED, OUTPUT);
    digitalWrite(pinWriteLED, LOW);
  }

  // Prepare write LED
  this->pinWriteLED  = pinWriteLED;
  if (pinWriteLED) {
    pinMode(pinWriteLED, OUTPUT);
    digitalWrite(pinWriteLED, LOW);
  }
}

/**
 * Activate/Deactivate debugger
 */
void CCDebugger::setActive( boolean on ) 
{

  // Reset error flag
  errorFlag = 0;

  // Continue only if active
  if (on == this->active) return;
  this->active = on;

  if (on) {

    // Prepare CC Pins
    pinMode(pinDC,        OUTPUT);
    pinMode(pinDD_I,      INPUT);
    pinMode(pinDD_O,      OUTPUT);
    pinMode(pinRST,       OUTPUT);
    digitalWrite(pinDC,   LOW);
    digitalWrite(pinDD_I, LOW); // No pull-up
    digitalWrite(pinDD_O, LOW);
    digitalWrite(pinRST,  LOW);

    // Activate leds
    if (pinReadLED) {
      pinMode(pinReadLED,       OUTPUT);
      digitalWrite(pinReadLED,  LOW);
    }
    if (pinWriteLED) {
      pinMode(pinWriteLED,      OUTPUT);
      digitalWrite(pinWriteLED, LOW);
    }

    // Default direction is INPUT
    setDDDirection(INPUT);

  } else {

    // Before deactivating, exit debug mode
    if (inDebugMode)
      this->exit();

    // Put everything in inactive mode
    pinMode(pinDC,        INPUT);
    pinMode(pinDD_I,      INPUT);
    pinMode(pinDD_O,      INPUT);
    pinMode(pinRST,       INPUT);
    digitalWrite(pinDC,   LOW);
    digitalWrite(pinDD_I, LOW);
    digitalWrite(pinDD_O, LOW);
    digitalWrite(pinRST,  LOW);

    // Deactivate leds
    if (pinReadLED) {
      pinMode(pinReadLED,       INPUT);
      digitalWrite(pinReadLED,  LOW);
    }
    if (pinWriteLED) {
      pinMode(pinWriteLED,      INPUT);
      digitalWrite(pinWriteLED, LOW);
    }
    
  }
}

/**
 * Return the error flag
 */
byte CCDebugger::error()
{
  return errorFlag;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
////                     LOW LEVEL FUNCTIONS                     ////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

/**
 * Delay a particular number of cycles
 */
void cc_delay( unsigned char d )
{
  volatile unsigned char i = d;
  while( i-- );
}

/**
 * Enter debug mode
 */
byte CCDebugger::enter() 
{
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (pinWriteLED) digitalWrite(pinWriteLED, HIGH);
  // =============

  // Reset error flag
  errorFlag = 0;

  // Enter debug mode
  digitalWrite(pinRST, LOW);
  cc_delay(200);
  digitalWrite(pinDC, HIGH);
  cc_delay(3);
  digitalWrite(pinDC, LOW);
  cc_delay(3);
  digitalWrite(pinDC, HIGH);
  cc_delay(3);
  digitalWrite(pinDC, LOW);
  cc_delay(200);
  digitalWrite(pinRST, HIGH);
  cc_delay(200);

  // We are now in debug mode
  inDebugMode = 1;

  // =============
  if (pinWriteLED) digitalWrite(pinWriteLED, LOW);

  // Success
  return 0;

};

/**
 * Write a byte to the debugger
 */
byte CCDebugger::write( byte data ) 
{
  if (!active) {
    errorFlag = 1;
    return 0;
  };
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }
  if (pinWriteLED) digitalWrite(pinWriteLED, HIGH);
  // =============

  byte cnt;

  // Make sure DD is on output
  setDDDirection(OUTPUT);

  // Sent bytes
  for (cnt = 8; cnt; cnt--) {

    // First put data bit on bus
    if (data & 0x80)
      digitalWrite(pinDD_O, HIGH);
    else
      digitalWrite(pinDD_O, LOW);

    // Place clock on high (other end reads data)
    digitalWrite(pinDC, HIGH);

    // Shift & Delay
    data <<= 1;
    cc_delay(2);

    // Place clock down
    digitalWrite(pinDC, LOW);
    cc_delay(2);

  }

  // =============
  if (pinWriteLED) digitalWrite(pinWriteLED, LOW);
  return 0;
}

/**
 * Wait until input is ready for reading
 */
byte CCDebugger::switchRead()
{
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }
  if (pinReadLED) digitalWrite(pinReadLED, HIGH);
  // =============

  byte cnt;
  byte didWait = 0;

  // Switch to input
  setDDDirection(INPUT);

  // Wait at least 83 ns before checking state t(dir_change)
  cc_delay(2);

  // Wait for DD to go LOW (Chip is READY)
  while (digitalRead(pinDD_I) == HIGH) {

    // Do 8 clock cycles
    for (cnt = 8; cnt; cnt--) {
      digitalWrite(pinDC, HIGH);
      cc_delay(2);
      digitalWrite(pinDC, LOW);
      cc_delay(2);
    }

    // Let next function know that we did wait
    didWait = 1;
  }

  // Wait t(sample_wait) 
  if (didWait) cc_delay(2);

  // =============
  if (pinReadLED) digitalWrite(pinReadLED, LOW);
  return 0;
}

/**
 * Switch to output
 */
byte CCDebugger::switchWrite()
{
  setDDDirection(OUTPUT);
  return 0;
}

/**
 * Read an input byte
 */
byte CCDebugger::read()
{
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (pinReadLED) digitalWrite(pinReadLED, HIGH);
  // =============

  byte cnt;
  byte data = 0;

  // Switch to input
  setDDDirection(INPUT);

  // Send 8 clock pulses if we are HIGH
  for (cnt = 8; cnt; cnt--) {
    digitalWrite(pinDC, HIGH);
    cc_delay(2);

    // Shift and read
    data <<= 1;
    if (digitalRead(pinDD_I) == HIGH)
      data |= 0x01;

    digitalWrite(pinDC, LOW);
    cc_delay(2);
  }

  // =============
  if (pinReadLED) digitalWrite(pinReadLED, LOW);

  return data;
}

/**
 * Switch reset pin
 */
void CCDebugger::setDDDirection( byte direction )
{

  // Switch direction if changed
  if (direction == ddIsOutput) return;
  ddIsOutput = direction;

  // Handle new direction
  if (ddIsOutput) {
    digitalWrite(pinDD_I, LOW); // Disable pull-up
    pinMode(pinDD_O, OUTPUT);   // Enable output
    digitalWrite(pinDD_O, LOW); // Switch to low
  } else {
    digitalWrite(pinDD_I, LOW); // Disable pull-up
    pinMode(pinDD_O, INPUT);    // Disable output
    digitalWrite(pinDD_O, LOW); // Don't use output pull-up
  }

}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
////                    HIGH LEVEL FUNCTIONS                     ////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////


/**
 * Exit from debug mode
 */
byte CCDebugger::exit() 
{
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x48 ); // RESUME
  switchRead();
  bAns = read(); // debug status
  switchWrite(); 

  inDebugMode = 0;

  return 0;
}
/**
 * Get debug configuration
 */
byte CCDebugger::getConfig() {
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x20 ); // RD_CONFIG
  switchRead();
  bAns = read(); // Config
  switchWrite(); 

  return bAns;
}

/**
 * Set debug configuration
 */
byte CCDebugger::setConfig( byte config ) {
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x18 ); // WR_CONFIG
  write( config );
  switchRead();
  bAns = read(); // Config
  switchWrite();

  return bAns;
}

/**
 * Invoke a debug instruction with 1 opcode
 */
byte CCDebugger::exec( byte oc0 )
{
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x51 ); // DEBUG_INSTR + 1b
  write( oc0 );
  switchRead();
  bAns = read(); // Accumulator
  switchWrite(); 

  return bAns;
}

/**
 * Invoke a debug instruction with 2 opcodes
 */
byte CCDebugger::exec( byte oc0, byte oc1 )
{
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x52 ); // DEBUG_INSTR + 2b
  write( oc0 );
  write( oc1 );
  switchRead();
  bAns = read(); // Accumulator
  switchWrite(); 

  return bAns;
}

/**
 * Invoke a debug instruction with 3 opcodes
 */
byte CCDebugger::exec( byte oc0, byte oc1, byte oc2 )
{
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x53 ); // DEBUG_INSTR + 3b
  write( oc0 );
  write( oc1 );
  write( oc2 );
  switchRead();
  bAns = read(); // Accumulator
  switchWrite(); 

  return bAns;
}

/**
 * Invoke a debug instruction with 1 opcode + 16-bit immediate
 */
byte CCDebugger::execi( byte oc0, unsigned short c0 )
{
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x53 ); // DEBUG_INSTR + 3b
  write( oc0 );
  write( (c0 >> 8) & 0xFF );
  write(  c0 & 0xFF );
  switchRead();
  bAns = read(); // Accumulator
  switchWrite(); 

  return bAns;
}

/**
 * Return chip ID
 */
unsigned short CCDebugger::getChipID() {
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  unsigned short bAns;
  byte bRes;

  write( 0x68 ); // GET_CHIP_ID
  switchRead();
  bRes = read(); // High order
  bAns = bRes << 8;
  bRes = read(); // Low order
  bAns |= bRes;
  switchWrite(); 

  return bAns;
}

/**
 * Return PC
 */
unsigned short CCDebugger::getPC() {
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  unsigned short bAns;
  byte bRes;

  write( 0x28 ); // GET_PC
  switchRead();
  bRes = read(); // High order
  bAns = bRes << 8;
  bRes = read(); // Low order
  bAns |= bRes;
  switchWrite(); 

  return bAns;
}

/**
 * Return debug status
 */
byte CCDebugger::getStatus() {
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x30 ); // READ_STATUS
  switchRead();
  bAns = read(); // debug status
  switchWrite(); 

  return bAns;
}

/**
 * Step instruction
 */
byte CCDebugger::step() {
  if (!active) {
    errorFlag = 1;
    return 0;
  }
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x58 ); // STEP_INSTR
  switchRead();
  bAns = read(); // Accumulator
  switchWrite(); 

  return bAns;
}

/**
 * Mass-erase all chip configuration & Lock Bits
 */
byte CCDebugger::chipErase()
{
  if (!active) {
    errorFlag = 1;
    return 0;
  };
  if (!inDebugMode) {
    errorFlag = 2;
    return 0;
  }

  byte bAns;

  write( 0x10 ); // CHIP_ERASE
  switchRead();
  bAns = read(); // Debug status
  switchWrite(); 

  return bAns;
};
