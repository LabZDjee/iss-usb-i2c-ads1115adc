/*
  bunch of unit tests for implementation of access
  to ADS1115-based Anix board
  this is code very much tied too the actual hardware
  it is targeted to, so not much explained here
  only stored for records
 */

#include <stdio.h>

typedef unsigned char byte;
typedef unsigned short word;

#include <string.h>

#include "bitfieldHandling.h"

byte __eeprom[4096];

byte EepromReadByte(word address)
{
 return __eeprom[address];
}

int EepromWriteByte(word address, byte data)
{
 if(address>4096) {
  return -1;
 }
 if(EepromReadByte(address)==data) {
  return 0;
 }
 __eeprom[address] = data;
 return 1;
}

#define ADDR_NMP_ANIX 0x4ef
#define ADDR_NMP_ANIX_LENGTH 17

void set_eeprom(byte val)
{
 int i=0;
 for(i=0; i<4096; i++) {
  __eeprom[i] = val;
 }
}


typedef struct _tAnix {
 byte device;         // 0-3, should be a constant
 byte sequenceIndex;  // goes from 0 to sequenceLength - 1
 byte sequenceLength; // how many steps (0: device disabled, 2: two diff, 3: one diff, two single, 4: four single)
 word adsConfigReg;   // configuration to send to the ADS1115
 word filter;         // 1 to 16: divider of IIR low-pass filter
 byte polarity;       // 1: unipolar [0; 32,767], 2: bipolar [-32,768; 32,767]
 short signed lastReading[4];
} tAnix;

/*
 ADS1115 definitions (see its data sheet section 9.6.3 for more)
 Internally used by ANIX module, only exported for possible debug purposes
*/

// Constant part of configuration register (high and low bytes)
#define ADS1115_CONFIGREG_0_LOW  (4<<5) // 128 SPS
#define ADS1115_CONFIGREG_0_HIGH (0)    // continous conversion

#define ADS_MUX_AIN0_AIN1 (0)
#define ADS_MUX_AIN0_AIN3 (1)
#define ADS_MUX_AIN1_AIN3 (2)
#define ADS_MUX_AIN2_AIN3 (3)
#define ADS_MUX_AIN0_GND  (4)
#define ADS_MUX_AIN1_GND  (5)
#define ADS_MUX_AIN2_GND  (6)
#define ADS_MUX_AIN3_GND  (7)

#define ADS_MUX_OFFSET_16 (12)
#define ADS_MUX_OFFSET_8  (ADS_MUX_OFFSET_16-8)
#define ADS_PGA_OFFSET_16 (9)
#define ADS_PGA_OFFSET_8  (ADS_PGA_OFFSET_16-8)

/*
 init tAnix struct, given a device index [0; 3]:
  field device set properly
  fields sequenceIndex and sequenceLength set to zero (this last one meaning: management disabled)
  reading fields are reset
  all other fields set to all ones (illegal values)
*/
void initAnixStruct(byte device, tAnix* pAnix)
 {
  int i;
  pAnix->device = device & 3;
  pAnix->sequenceIndex=pAnix->sequenceLength=0;
  pAnix->polarity=0xff;
  pAnix->adsConfigReg=pAnix->filter=0xffff;
  for(i=0; i<4; i++)
   {
    pAnix->lastReading[i] = 0;
   }
 }

/*
 update tAnix struct with contents of the Eeprom configuration
 takes fields device and sequenceIndex into account
  if field sequenceIndex is out-of-bounds, it is set to zero (which is handy as user
  can safely increment this field without caring about overflow)
 all fields within this structure could be updated
*/
void syncAnixStruct(tAnix* pAnix)
 {
  byte device = pAnix->device;
  byte deviceCompressedConfig = EepromReadByte(ADDR_NMP_ANIX+ADDR_NMP_ANIX_LENGTH-1);
  pAnix->adsConfigReg = (ADS1115_CONFIGREG_0_HIGH << 8) + ADS1115_CONFIGREG_0_LOW;
  switch(TAKE_BITS(deviceCompressedConfig, 2, device<<1))
   {
    case 0:
     initAnixStruct(device, pAnix);
     return;
    case 1:
     pAnix->sequenceLength = 2;
     if(pAnix->sequenceIndex == 1)
      {
       pAnix->adsConfigReg |= ADS_MUX_AIN2_AIN3 << ADS_MUX_OFFSET_16;
      }
     else
      {
       pAnix->sequenceIndex = 0;
       pAnix->adsConfigReg |= ADS_MUX_AIN0_AIN1 << ADS_MUX_OFFSET_16;
      }
     break;
    case 2:
     pAnix->sequenceLength = 3;
     switch(pAnix->sequenceIndex)
      {
       default:
        pAnix->sequenceIndex=0;
        // no break on purpose
       case 0:
        pAnix->adsConfigReg |= ADS_MUX_AIN0_AIN1 << ADS_MUX_OFFSET_16;
        break;
       case 1:
        pAnix->adsConfigReg |= ADS_MUX_AIN2_GND << ADS_MUX_OFFSET_16;
        break;
       case 2:
        pAnix->adsConfigReg |= ADS_MUX_AIN3_GND << ADS_MUX_OFFSET_16;
        break;
      }
     break;
    case 3:
     pAnix->sequenceLength = 4;
     switch(pAnix->sequenceIndex)
      {
       default:
        pAnix->sequenceIndex=0;
        // no break on purpose
       case 0:
        pAnix->adsConfigReg |= ADS_MUX_AIN0_GND << ADS_MUX_OFFSET_16;
        break;
       case 1:
        pAnix->adsConfigReg |= ADS_MUX_AIN1_GND << ADS_MUX_OFFSET_16;
        break;
       case 2:
        pAnix->adsConfigReg |= ADS_MUX_AIN2_GND << ADS_MUX_OFFSET_16;
        break;
       case 3:
        pAnix->adsConfigReg |= ADS_MUX_AIN3_GND << ADS_MUX_OFFSET_16;
        break;
      }
     break;
   }
  deviceCompressedConfig = EepromReadByte(ADDR_NMP_ANIX+(device<<2)+pAnix->sequenceIndex);
  if(deviceCompressedConfig == 0xff) // probably un-initialized EEPROM
   {
    initAnixStruct(device, pAnix);
    return;
   }
  pAnix->adsConfigReg |= TAKE_BITS(deviceCompressedConfig, 3, 4)  << ADS_PGA_OFFSET_16;
  pAnix->filter = TAKE_BITS(deviceCompressedConfig, 4, 0) + 1;
  pAnix->polarity = TAKE_BITS(deviceCompressedConfig, 1, 7) + 1;
 }

// storage in EEPROM
// 17 bytes
//  4x4 bytes:
//   Pol:7 Gain:6-4[1-5] IIRF:3-0[0-15]
//  1 byte: Prof3:7-6 Prof2:5-4 Prof1:3-2 Prof0:1-0
//   

typedef struct _tAnixTempConfig{
 byte profile; // profile 0, 2, 3, 4, 0xff: undefined)
 word gain[4]; // gains (4096, 2048... 0xffff: undefined)
 byte iir[4];  // iir filter take percentages (1 to 16, 0xff undefined)
 byte pol[4];  // polarities (1, 2, 0xff: undefinded)
} tAnixTempConfig;

int setAnixConfigInEEprom(byte anixIndex, tAnixTempConfig* pAtp, byte dryRun)
{
 byte b, v, n;
 if(anixIndex>3)
 {
  return -1;
 }
 b = EepromReadByte(ADDR_NMP_ANIX+ADDR_NMP_ANIX_LENGTH-1);
 v = pAtp->profile;
 switch (v)
  {
   case 0:
    goto _assign_and_write_prof;
   case 2:
   case 3:
   case 4:
    v--;
   _assign_and_write_prof:
    ASSIGN_BITS(b, v, 2, anixIndex<<1);
    if(dryRun == 0)
     {
      EepromWriteByte(ADDR_NMP_ANIX+ADDR_NMP_ANIX_LENGTH-1, b);
     }
    break;
   case 255:
    break;
   default:
    return 1;
  }
 for(n=0; n<4; n++)
  {
   b = EepromReadByte(ADDR_NMP_ANIX+(anixIndex<<2)+n);
   switch(pAtp->pol[n])
    {
     case 1:
      v = 0;
      goto _assign_pol;
     case 2:
      v = 1;
     _assign_pol:
      ASSIGN_BITS(b, v, 1, 7);
      break;
     case 255:
      break;
     default:
      return 10+n;
    }
   switch(pAtp->gain[n])
    {
     case 4096:
      v = 1;
      goto _assign_gain;
     case 2048:
      v = 2;
      goto _assign_gain;
     case 1024:
      v = 3;
      goto _assign_gain;
     case 512:
      v = 4;
      goto _assign_gain;
     case 256:
      v = 5;
     _assign_gain:
      ASSIGN_BITS(b, v, 3, 4);
      break;
     case 65535:
      break;
     default:
      return 2+n;
    }
   v = pAtp->iir[n];
   if(v>=1 && v<=16)
    {
     ASSIGN_BITS(b, v-1, 4, 0);
    }
     else if(v != 255)
    {
     return 6+n;
    }
    if(dryRun == 0)
     {
      EepromWriteByte(ADDR_NMP_ANIX+(anixIndex<<2)+n, b);
     }
  }
return 0;
}

void getAnixConfigFromEeprom(byte anixIndex, tAnixTempConfig* pAtp)
 {
  byte b, n;
  if(anixIndex>3)
   {
    return;
   }
  b=TAKE_BITS(EepromReadByte(ADDR_NMP_ANIX+ADDR_NMP_ANIX_LENGTH-1), 2, anixIndex<<1);
  pAtp->profile = b + (b ? 1 : 0);
  for(n=0; n<4; n++)
   {
    b=EepromReadByte(ADDR_NMP_ANIX+(anixIndex<<2)+n);
    pAtp->gain[n] = 1<<(13-TAKE_BITS(b, 3, 4));
    pAtp->iir[n] = TAKE_BITS(b, 4, 0) + 1;
    pAtp->pol[n] = TAKE_BITS(b, 1, 7) + 1;
   }
 }

int correctAnixInEEprom() {
 int i;
 for(i=0; i<ADDR_NMP_ANIX_LENGTH-1; i++)
  {
   if(EepromReadByte(ADDR_NMP_ANIX+i) == 0xff)
    {
     for(i=0; i<ADDR_NMP_ANIX_LENGTH-1; i++)
      {
       EepromWriteByte(ADDR_NMP_ANIX+i, 0x10); // default: positive, 4,096mV, 100% 
      }
     EepromWriteByte(ADDR_NMP_ANIX+i, 0); // all disabled
     return !0;
    }
  }
 return 0;
}

/////////////////////////////////////////////
// Test Zone
/////////////////////////////////////////////

tAnix anix[4];

void printEeprom()
 {
  int i, j;
  word addr = ADDR_NMP_ANIX;
  byte contents;
  for(i=0; i<4; i++)
   {
    for(j=0; j<4; j++)
     {
      contents = EepromReadByte(addr);
      printf("0x%02x ", contents);
      addr++;
     }
    printf("\n");
   } 
  contents = EepromReadByte(addr);
  printf("0x%02x\n", contents);
 }

char* splitADSRegForDisplay(word reg, char* buffer)
 {
  sprintf(buffer, "OS:%iMUX:%iPGA:%iMODE:%i/DR:%iCOMP_MODE:%iCOMP_POL:%iCOMP_LAT:%iCOMP_QUE:%i",
          TAKE_BITS(reg, 1, 15),
          TAKE_BITS(reg, 3, 12),
          TAKE_BITS(reg, 3, 9),
          TAKE_BITS(reg, 1, 8),
          TAKE_BITS(reg, 3, 5),
          TAKE_BITS(reg, 1, 4),
          TAKE_BITS(reg, 1, 3),
          TAKE_BITS(reg, 1, 2),
          TAKE_BITS(reg, 2, 0));
  return buffer;
 }

void printAnixStruct(tAnix* pAnix)
 {
  char buf[80];
  if(pAnix->sequenceLength == 0)
   {
    printf("dev: %i is disabled\n", (int)pAnix->device);
    return;
   }
  printf("dev: %i seq: %i/%i ads cfg[%s] f: %i pol: %i\n",
   (int)pAnix->device,
   (int)pAnix->sequenceIndex+1, (int)pAnix->sequenceLength,
   splitADSRegForDisplay(pAnix->adsConfigReg, buf),
   (int)pAnix->filter,
   (int)pAnix->polarity
  );
 }

void testTAnix()
 {
  int i;
  int dev;
  for(dev=0; dev<4; dev++)
   {
    initAnixStruct(dev, anix+dev);
   }
  for(i=0; i<5; i++)
   {
    for(dev=0; dev<4; dev++)
     {
      syncAnixStruct(anix+dev);
      printAnixStruct(anix+dev);
      anix[dev].sequenceIndex++;
     }
   }
 }

tAnixTempConfig tests_1[] = {
  {profile: 4, gain: {4096, 1024, 256, 512}, iir: {1, 2, 3, 4}, pol: {1, 2, 2, 1}},
  {profile: 2, gain: {256, 512, 4096, 4096}, iir: {11, 12, 1, 1}, pol: {2, 2, 1, 1}},
  {profile: 0, gain: {4096, 4096, 4096, 4096}, iir: {1, 1, 1, 1}, pol: {1, 1, 1, 1}},
  {profile: 3, gain: {256, 512, 1024, 4096}, iir: {7, 8, 9, 1}, pol: {2, 1, 1, 1}}
};

byte tests_comp_1[][1+ADDR_NMP_ANIX_LENGTH] = {{2,
                         255, 255, 255, 255,
                         255, 255, 255, 255,
                         16*1+0, 128+16*3+1, 128+16*5+2, 16*4+3,
                         255, 255, 255, 255,
                         3<<4},
                         {1,
                         255, 255, 255, 255,
                         128+16*5+10, 128+16*4+11, 16, 16,
                         255, 255, 255, 255,
                         255, 255, 255, 255,
                         1<<2},
                        {3,
                         255, 255, 255, 255,
                         255, 255, 255, 255,
                         255, 255, 255, 255,
                         16, 16, 16, 16,
                         0},
                        {0,
                         128+16*5+6, 16*4+7, 16*3+8, 16,
                         255, 255, 255, 255,
                         255, 255, 255, 255,
                         255, 255, 255, 255,
                         2<<0}
};

tAnixTempConfig tests_2[] = {
  {profile: 3, gain: {0xffff, 0xffff, 0xffff, 0xffff}, iir: {0xff, 0xff, 0xff, 0xff}, pol: {0xff, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {2048, 0xffff, 0xffff, 0xffff}, iir: {0xff, 0xff, 0xff, 0xff}, pol: {0xff, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 512, 0xffff, 0xffff}, iir: {0xff, 0xff, 0xff, 0xff}, pol: {0xff, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 256, 0xffff}, iir: {0xff, 0xff, 0xff, 0xff}, pol: {0xff, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 0xffff, 1024}, iir: {0xff, 0xff, 0xff, 0xff}, pol: {0xff, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 0xffff, 0xffff}, iir: {11, 0xff, 0xff, 0xff}, pol: {0xff, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 0xffff, 0xffff}, iir: {0xff, 7, 0xff, 0xff}, pol: {0xff, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 0xffff, 0xffff}, iir: {0xff, 0xff, 4, 0xff}, pol: {0xff, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 0xffff, 0xffff}, iir: {0xff, 0xff, 0xff, 3}, pol: {0xff, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 0xffff, 0xffff}, iir: {0xff, 0xff, 0xff, 0xff}, pol: {1, 0xff, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 0xffff, 0xffff}, iir: {0xff, 0xff, 0xff, 0xff}, pol: {0xff, 2, 0xff, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 0xffff, 0xffff}, iir: {0xff, 0xff, 0xff, 0xff}, pol: {0xff, 0xff, 1, 0xff}},
  {profile: 0xff, gain: {0xffff, 0xffff, 0xffff, 0xffff}, iir: {0xff, 0xff, 0xff, 0xff}, pol: {0xff, 0xff, 0xff, 2}},
};

byte test_comp_2[][5] = {
 {2, 16, 2, 2, 4}, // device, offset in EEprom, value, value width, value offset
 {3, 3*4, 2, 3, 4},
 {1, 1*4+1, 4, 3, 4},
 {0, 2, 5, 3, 4},
 {3, 3*4+3, 3, 3, 4},
 {1, 1*4, 10, 4, 0},
 {3, 3*4+1, 6, 4, 0},
 {0, 2, 3, 4, 0},
 {2, 2*4+3, 2, 4, 0},
 {0, 0, 0, 1, 7},
 {1, 1*4+1, 1, 1, 7},
 {2, 2*4+2, 0, 1, 7},
 {3, 3*4+3, 1, 1, 7}
};

tAnixTempConfig tests_3[] = {
  {profile: 5, gain: {4096, 1024, 256, 512}, iir: {1, 2, 3, 4}, pol: {1, 2, 2, 1}},
  {profile: 2, gain: {255, 512, 4096, 4096}, iir: {11, 12, 1, 1}, pol: {2, 2, 1, 1}},
  {profile: 0, gain: {4096, 4095, 4096, 4096}, iir: {1, 1, 1, 1}, pol: {1, 1, 1, 1}},
  {profile: 3, gain: {256, 512, 1023, 4096}, iir: {7, 8, 9, 1}, pol: {2, 1, 1, 1}},
  {profile: 4, gain: {4096, 1024, 256, 511}, iir: {1, 2, 3, 4}, pol: {1, 2, 2, 1}},
  {profile: 2, gain: {256, 512, 4096, 4096}, iir: {17, 12, 1, 1}, pol: {2, 2, 1, 1}},
  {profile: 0, gain: {4096, 4096, 4096, 4096}, iir: {1, 18, 1, 1}, pol: {1, 1, 1, 1}},
  {profile: 3, gain: {256, 512, 1024, 4096}, iir: {7, 8, 0, 1}, pol: {2, 1, 1, 1}},
  {profile: 4, gain: {4096, 1024, 256, 512}, iir: {1, 2, 3, 40}, pol: {1, 2, 2, 1}},
  {profile: 2, gain: {256, 512, 4096, 4096}, iir: {11, 12, 1, 1}, pol: {3, 2, 1, 1}},
  {profile: 0, gain: {4096, 4096, 4096, 4096}, iir: {1, 1, 1, 1}, pol: {1, 0, 1, 1}},
  {profile: 3, gain: {256, 512, 1024, 4096}, iir: {7, 8, 9, 1}, pol: {2, 1, 22, 1}},
  {profile: 3, gain: {256, 512, 1024, 4096}, iir: {7, 8, 9, 1}, pol: {2, 1, 1, 11}}
};

char* test_getAnixConfigFromEeprom(byte anixIndex, tAnixTempConfig* pAtp)
 {
  int n;
  static char msg[64];
  tAnixTempConfig t;
  getAnixConfigFromEeprom(anixIndex, &t);
  if(pAtp->profile!=255 && t.profile != pAtp->profile)
   {
    sprintf(msg, "test_getAnixConfigFromEeprom: failure in profile, expected: %i, got: %i", pAtp->profile, t.profile);
    return msg;
   }
  for(n=0; n<4; n++)
   {
    if(pAtp->gain[n] != 0xffff && pAtp->gain[n] != t.gain[n])
     {
      sprintf(msg, "test_getAnixConfigFromEeprom: failure in gain #%i, expected: %i, got: %i",
              n+1, pAtp->gain[n], t.gain[n]);
      return msg; 
     }
    if(pAtp->iir[n] != 255 && pAtp->iir[n] != t.iir[n])
     {
      sprintf(msg, "test_getAnixConfigFromEeprom: failure in iir filter #%i, expected: %i, got: %i",
              n+1, pAtp->iir[n], t.iir[n]);
      return msg; 
     }
    if(pAtp->pol[n] != 255 && pAtp->pol[n] != t.pol[n])
     {
      sprintf(msg, "test_getAnixConfigFromEeprom: failure in polarity #%i, expected: %i, got: %i",
              n+1, pAtp->pol[n], t.pol[n]);
      return msg; 
     }
   }
  return NULL;
 }

void makeBlankEepromTestVector(byte vect[ADDR_NMP_ANIX_LENGTH])
{
 memset(vect, 0xff, ADDR_NMP_ANIX_LENGTH);
}

void injectOneValueIntoEepromTestVector(byte def[5], byte vect[ADDR_NMP_ANIX_LENGTH]) // def: device, offset in EEprom, value, value width, value offset
{
 byte value = vect[def[1]];
 ASSIGN_BITS(value, def[2], def[3], def[4]);
 vect[def[1]] = value;
}

int compareAgainstEepromTestVector(byte vect[ADDR_NMP_ANIX_LENGTH])
{
 int i;
 for(i=0; i<ADDR_NMP_ANIX_LENGTH; i++)
  {
   if(EepromReadByte(ADDR_NMP_ANIX+i) != vect[i])
    {
     return i;
    }
  }
 return -1;
}

int checkEepromConsistency()
{
 int i;
 for(i=0; i<16; i++)
 {
  byte gain = TAKE_BITS(EepromReadByte(ADDR_NMP_ANIX+i), 3, 4);
  if(gain == 0 || gain > 5)
  {
   return 0;
  }
 }
 return !0;
}

void main()
{
 int n, w, t;
 char* pc;
 byte eepromTestVector[ADDR_NMP_ANIX_LENGTH];
 set_eeprom(0xff);
 if(!correctAnixInEEprom())
  {
   printf("failure: correctAnixInEEprom should return true on blank EEPROM\n");
   return;
  }
 if(EepromReadByte(ADDR_NMP_ANIX+16) != 0)
  {
   printf("failure: correctAnixInEEprom returning true should leave all four ANIX boards disabled\n");
   return;
  }
 if(checkEepromConsistency() == 0)
  {
   printf("failure: EEPROM not consistent after correctAnixInEEprom\n");
   return;
  }
 if(correctAnixInEEprom())
  {
   printf("failure: correctAnixInEEprom should return false on correct EEPROM contents\n");
   return;
  }
 for(n=0; n<sizeof(tests_2)/sizeof(*tests_2); n++)
  {
   set_eeprom(0xff);
   makeBlankEepromTestVector(eepromTestVector);
   setAnixConfigInEEprom(test_comp_2[n][0], tests_2+n, 0);
   injectOneValueIntoEepromTestVector(test_comp_2[n], eepromTestVector);
   if((t=compareAgainstEepromTestVector(eepromTestVector))>=0)
    {
     printf("failure at step 2.%i: EEprom at offset 0x%04x+%i at value 0x%02x instead of 0x%02x\n",
             n+1, ADDR_NMP_ANIX, t, EepromReadByte(ADDR_NMP_ANIX+t), eepromTestVector[t]);
     return;
    }
  }
 for(n=0; n<sizeof(tests_2)/sizeof(*tests_2); n++)
  {
   if(setAnixConfigInEEprom(n%4, tests_3+n, 1) != n+1)
    {
     printf("failure at step 3.%i as setAnixConfigInEEprom failed to return a proper error\n", n+1);
     return;
    }
  }
 for(n=0; n<sizeof(tests_1)/sizeof(*tests_1); n++)
  {
   set_eeprom(0xff);
   makeBlankEepromTestVector(eepromTestVector);
   setAnixConfigInEEprom(tests_comp_1[n][0], tests_1+n, 1);
   if((t=compareAgainstEepromTestVector(eepromTestVector))>=0)
    {
     printf("failure EEprom altered at offset 0x%04x+%i when setAnixConfigInEEprom called as a dry run\n", ADDR_NMP_ANIX, t);
     return;
    }
   EepromWriteByte(ADDR_NMP_ANIX+ADDR_NMP_ANIX_LENGTH-1, 0);
   setAnixConfigInEEprom(tests_comp_1[n][0], tests_1+n, 0);
   for(w=0; w<ADDR_NMP_ANIX_LENGTH; w++)
    {
     if(tests_comp_1[n][w+1] != 255)
      {
       if(tests_comp_1[n][w+1] != EepromReadByte(w+ADDR_NMP_ANIX))
        {
         printf("failure on setAnixConfigInEEprom at step 1.%i, offset %i, expected: 0x%02x, got: 0x%02x\n",
               n+1, w, tests_comp_1[n][w+1], EepromReadByte(w+ADDR_NMP_ANIX));
        }
      }
    }
   if((pc=test_getAnixConfigFromEeprom(tests_comp_1[n][0], tests_1+n))!=NULL)
    {
     printf("at step 1.%i: %s\n", n+1, pc);
    }
  }
   printEeprom();
   testTAnix();
}
