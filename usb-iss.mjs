/* jshint esversion: 6 */

let comPortPath = "/dev/ttyS0";

const mode = "I2C_S_20KHZ";

if (process.argv.length > 2) {
  comPortPath = process.argv[2];
}

import SerialPort from "serialport";

// no special parser as port.pipe, so data is a Node.js stream which deals with 'Buffer' objects
const port = new SerialPort(comPortPath, {
  autoOpen: false,
});

import u from "./utility.mjs";

const usbIssCmd = 0x5a;

const issSubCmds = {
  ISS_VERSION: 1,
  ISS_MODE: 2,
  GET_SER_NUM: 3,
};

const i2cMode = {
  I2C_SGL: 0x53,
  I2C_AD0: 0x54,
  I2C_AD1: 0x55,
  I2C_AD2: 0x56,
  I2C_DIRECT: 0x57,
  I2C_TEST: 0x58,
};

const i2cDirectCommands = {
  I2C_Start: 0x01,
  I2C_Restart: 0x02,
  I2C_Stop: 0x03,
  I2C_Nack: 0x04,
  I2C_Read1: 0x20, // add 1 for 2 bytes up to 15 for 16 bytes maximum
  I2C_Write1: 0x30, // add 1 for 2 bytes up to 15 for 16 bytes maximum
};

const issModes = {
  IO_MODE: 0x00,
  IO_CHANGE: 0x10,
  I2C_S_20KHZ: 0x20,
  I2C_S_50KHZ: 0x30,
  I2C_S_100KHZ: 0x40,
  I2C_S_400KHZ: 0x50,
  I2C_H_100KHZ: 0x60,
  I2C_H_400KHZ: 0x70,
  I2C_H_1000KHZ: 0x80,
  SPI_MODE: 0x90,
  SERIAL: 0x01,
};

function getIssModeFromValue(val) {
  return u.v2k(val, issModes, "???");
}

function bufferToArray(buffer) {
  return Array.from(buffer.values());
}

function bufferToString(buffer) {
  return buffer.toString();
}

let onDataStepName;

function nextStep(stepName, valueAsArray) {
  port.write(Buffer.from(valueAsArray));
  onDataStepName = stepName;
}

port.open(function(err) {
  if (err) {
    console.log(`Error opening port '${comPortPath}'!`);
    console.log(`Note: port com path/name can be passed as command line parameter`);
    console.log("      for your convenience, let's try to enumerate available port(s):");
    SerialPort.list().then(
      (ports) => {
        let nbOfPorts = 0;
        ports.forEach((port) => console.log(`       #${++nbOfPorts}: ${port.comName} (${port.pnpId})`));
        if (nbOfPorts === 0) {
          console.log("        none found!");
        }
        process.exit();
      },
      (err) => {
        console.log(` Failure! ${err}`);
        process.exit();
      }
    );
    return;
  }
  console.log(`Port '${comPortPath}' open`);
  console.log(`Set USB-ISS mode to: '${mode}'`);
  nextStep("setIssMode", [usbIssCmd, issSubCmds.ISS_MODE, issModes[mode]]);
});

let portOnDataExtendedFunction = null;
let continuationStep = null;
let continuationByteArray = [];

// prototype of onDataFunction: (step: string, data: Buffer) => true if end of program
function setContinuation(firstStep, firstByteArray, onDataFunction) {
  continuationStep = firstStep;
  continuationByteArray = firstByteArray;
  portOnDataExtendedFunction = onDataFunction;
}

port.on("data", function(data) {
  switch (onDataStepName) {
    case "setIssMode":
      const setModeReply = bufferToArray(data);
      if (setModeReply[0] === 255) {
        console.log(" Set USB-ISS Mode Okay");
      } else {
        console.log(` Set USB-ISS Mode returned error! (code: ${setModeReply[1]})`);
      }
      nextStep("getIssVersion", [usbIssCmd, issSubCmds.ISS_VERSION]);
      break;
    case "getIssVersion":
      const versionReply = bufferToArray(data);
      console.log(
        `Module id: ${versionReply[0]}, firmware version: ${versionReply[1]}, mode: ${getIssModeFromValue(
          versionReply[2]
        )} (0x${versionReply[2].toString(16)})`
      );
      nextStep("getUsbSerialNumber", [usbIssCmd, issSubCmds.GET_SER_NUM]);
      break;
    case "getUsbSerialNumber":
      const usbSerialNumberReply = bufferToString(data);
      console.log(`Module USB Serial Number: ${usbSerialNumberReply}`);
      if (continuationStep) {
        nextStep(continuationStep, continuationByteArray);
      } else {
        console.log("No continuation (probably called as standalone): exit process");
        process.exit();
      }
      break;
    default:
      let end = true;
      if (portOnDataExtendedFunction && typeof portOnDataExtendedFunction == "function") {
        end = !!portOnDataExtendedFunction(onDataStepName, data);
      }
      if (end) {
        process.exit();
      }
      break;
  }
});

export default {
  nextStep,
  setContinuation,
  bufferToArray,
  bufferToString,
  usbIssCmd,
  i2cMode,
  i2cDirectCommands,
};
