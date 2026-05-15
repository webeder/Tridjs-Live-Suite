// Function and var declaration
function PioneerDDJ1000() {}

// --------- Midi Absolute
const On = 0x7F;
const Off = 0x00;

// --------- For Tempo range
const Tempo8 = 0.08; // 8%
const Tempo12 = 0.12; // 12%
const Tempo24 = 0.24; // 24%
const Tempo100 = 1; // 100%
var actualTempoValueCH1 = 0;
var actualTempoValueCH2 = 0;
var actualTempoCH1 = 0;
var actualTempoCH2 = 0;

// --------- MidOut color Code
const blue = 1; 
const cyan = 9; 
const green = 21; 
const yellow = 29; 
const orange = 32; 
const red = 39; 
const mauve = 44;
const purple = 57;
const grey = 64;
const black = 0; // 0->dimmed ; 63->dark
var hotCueColor = green+3

// ---------- Display General
const displayOn = 0x00;
const displayOff = 0x7F;

// ---------- Jog Display
const positionBarSpeedFactor = 25; // if == 1, the position bar is very slow...
// ---------- FX


// ---------------------------------------------------------------------------------------------------------------
// ------------------------- Init & Shutdown ---------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------
PioneerDDJ1000.init = function (id, debugging) {
	var i = 0; // for iteration
	
	// Jog wheel information on
		// Jog CH 1
		midi.sendShortMsg(0x90,0x5B,0x01); // Light 
		midi.sendShortMsg(0x90,0x5D,displayOn); // Display
		midi.sendShortMsg(0x90,0x42,0); // clear minutes
		midi.sendShortMsg(0x90,0x43,0); // clear secondes
		// Jog CH 2
		midi.sendShortMsg(0x91,0x5B,0x01); // Light 
		midi.sendShortMsg(0x91,0x5D,displayOn); // Display
		midi.sendShortMsg(0x91,0x42,0); // clear minutes
		midi.sendShortMsg(0x91,0x43,0); // clear secondes
		
	// Set Tempo range
	engine.setValue("[Channel1]", "rateRange", Tempo8);
	engine.setValue("[Channel2]", "rateRange", Tempo8);
	
	actualTempoCH1 = Tempo8;
	actualTempoCH2 = Tempo8;
	// LED Off -> To do
	// HotCues off
	for (i=0;i==7;i++) {	
		midi.sendShortMsg(0x97,i,black);
		midi.sendShortMsg(0x99,i,black);
	}
	// Clean VuMeter Faders
	midi.sendShortMsg(0xB0,0x02,0);
	midi.sendShortMsg(0xB1,0x02,0);
	
	
	// ------------------- Callback functions --------------------------------------------
	// ------ Playposition !!!
	engine.connectControl("[Channel1]", "playposition","PioneerDDJ1000.TrackTimeUpdateChannel1");
	engine.connectControl("[Channel2]", "playposition","PioneerDDJ1000.TrackTimeUpdateChannel2");
	// ------ VuMeters
	engine.connectControl("[Channel1]","VuMeter","PioneerDDJ1000.LedVuMeterCH1");// Callback connection for VuMeters
	engine.connectControl("[Channel2]","VuMeter","PioneerDDJ1000.LedVuMeterCH2");// Callback connection for VuMeters
	// BPM On Jog
	engine.connectControl("[Channel1]","bpm","PioneerDDJ1000.BpmOnJogCH1");
	engine.connectControl("[Channel2]","bpm","PioneerDDJ1000.BpmOnJogCH2");
	// ------ HotCues
	engine.connectControl("[Channel1]","hotcue_1_enabled","PioneerDDJ1000.HotCuesUpdate1CH1");// HotCues 1 CH1
	engine.connectControl("[Channel1]","hotcue_2_enabled","PioneerDDJ1000.HotCuesUpdate2CH1");// HotCues 2 CH1
	engine.connectControl("[Channel1]","hotcue_3_enabled","PioneerDDJ1000.HotCuesUpdate3CH1");// HotCues 3 CH1
	engine.connectControl("[Channel1]","hotcue_4_enabled","PioneerDDJ1000.HotCuesUpdate4CH1");// HotCues 4 CH1
	engine.connectControl("[Channel1]","hotcue_5_enabled","PioneerDDJ1000.HotCuesUpdate5CH1");// HotCues 5 CH1
	engine.connectControl("[Channel1]","hotcue_6_enabled","PioneerDDJ1000.HotCuesUpdate6CH1");// HotCues 6 CH1
	engine.connectControl("[Channel1]","hotcue_7_enabled","PioneerDDJ1000.HotCuesUpdate7CH1");// HotCues 7 CH1
	engine.connectControl("[Channel1]","hotcue_8_enabled","PioneerDDJ1000.HotCuesUpdate8CH1");// HotCues 8 CH1
	
	engine.connectControl("[Channel2]","hotcue_1_enabled","PioneerDDJ1000.HotCuesUpdate1CH2");// HotCues 1 CH2
	engine.connectControl("[Channel2]","hotcue_2_enabled","PioneerDDJ1000.HotCuesUpdate2CH2");// HotCues 2 CH2
	engine.connectControl("[Channel2]","hotcue_3_enabled","PioneerDDJ1000.HotCuesUpdate3CH2");// HotCues 3 CH2
	engine.connectControl("[Channel2]","hotcue_4_enabled","PioneerDDJ1000.HotCuesUpdate4CH2");// HotCues 4 CH2
	engine.connectControl("[Channel2]","hotcue_5_enabled","PioneerDDJ1000.HotCuesUpdate5CH2");// HotCues 5 CH2
	engine.connectControl("[Channel2]","hotcue_6_enabled","PioneerDDJ1000.HotCuesUpdate6CH2");// HotCues 6 CH2
	engine.connectControl("[Channel2]","hotcue_7_enabled","PioneerDDJ1000.HotCuesUpdate7CH2");// HotCues 7 CH2
	engine.connectControl("[Channel2]","hotcue_8_enabled","PioneerDDJ1000.HotCuesUpdate8CH2");// HotCues 8 CH2
	
};

PioneerDDJ1000.shutdown = function() {
	var i = 0; // for iteration

	
	// Jog wheel information off
		// JOG CH1
		midi.sendShortMsg(0x90,0x42,0); // clear minutes
		midi.sendShortMsg(0x90,0x43,0); // clear secondes
		midi.sendShortMsg(0x90,0x5B,0x00); // Light
		midi.sendShortMsg(0x90,0x5D,displayOff); // Display off
		// JOG CH2
		midi.sendShortMsg(0x91,0x42,0); // clear minutes
		midi.sendShortMsg(0x91,0x43,0); // clear secondes
		midi.sendShortMsg(0x91,0x5B,0x00); // Light
		midi.sendShortMsg(0x91,0x5D,displayOff); // Display off
	
	// HotCues off
	for (i=0;i==7;i++) {	
		midi.sendShortMsg(0x97,i,black);
		midi.sendShortMsg(0x99,i,black);
	}
	
	// Clean VuMeter Faders
	midi.sendShortMsg(0xB0,0x02,0);
	midi.sendShortMsg(0xB1,0x02,0);
};

// ---------------------------------------------------------------------------------------------------------------
// ------------------------- All propose functions ----------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------

PioneerDDJ1000.sensitivityMinimizer = function (value, factor) { // factor must be lower than value !!!
		return (value/factor);
};

PioneerDDJ1000.sensitivityMaximizer = function (value, factor) { // factor value not to high !!!
		return (value*factor);
};


// ---------------------------------------------------------------------------------------------------------------
// ------------------------- JOG Wheel ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------

// ------------------------------ Scratch and Pitch bend ------------------------------

    // The button that enables/disables scratching
    PioneerDDJ1000.wheelTouch = function (channel, control, value, status, group) {  // David Goodenough
        var deckNumber = script.deckFromGroup(group);
      if (value == 0x7F) { 
            var alpha = 1.0/8;
            var beta = alpha/32;
            engine.scratchEnable(deckNumber, 32767, 33+1/3, alpha, beta);
        } else {    // If button up
            engine.scratchDisable(deckNumber);
        }
    };
     
    // The wheel that actually controls the scratching
    PioneerDDJ1000.wheelTurn = function (channel, control, value, status, group) { // David Goodenough
        var newValue = value - 64;
        var deckNumber = script.deckFromGroup(group);
		
        if (engine.isScratching(deckNumber)) {
            engine.scratchTick(deckNumber, PioneerDDJ1000.sensitivityMaximizer(newValue,1.5)); // Scratch!
        } else {
            //engine.setValue(group, 'jog', PioneerDDJ1000.bendValue(newValue)); // Pitch bend
			engine.setValue(group, 'jog', PioneerDDJ1000.sensitivityMinimizer(newValue,16)); // Pitch bend
        }
    };

// ------------------------------------------------------------------------------------------
// ------------------------------ Jog illumination/Information ------------------------------
// ------------------------------------------------------------------------------------------

	PioneerDDJ1000.TrackTimeUpdateChannel1 = function (value, group, control){    // For channel 1
		
		var trackDuration = engine.getValue(group, "duration"); // get total time
		var timeLeft = trackDuration * (1.0 - value); // compute time left
		var actualTime = trackDuration * value;
		var minutesLeft = timeLeft/60; 
		var secondesLeft = timeLeft%60; // modulo to get seconde...
		var actualMinutes = actualTime/60; 
		var actualSecondes = actualTime%60; // modulo to get seconde...
		
		midi.sendShortMsg(0x90,0x44,0x7F); // For remaining time
		midi.sendShortMsg(0x90,0x42,minutesLeft); // send minutes
		midi.sendShortMsg(0x90,0x43,secondesLeft); // send secondes
		
		PioneerDDJ1000.CurrentPositionBarUpdate(1,actualSecondes); // Update Position Bar for channel 1
	};
	
	PioneerDDJ1000.TrackTimeUpdateChannel2 = function (value, group, control) {   // For channel 2
		
		var trackDuration = engine.getValue(group, "duration"); // get total time
		var timeLeft = trackDuration * (1.0 - value); // compute time left
		var actualTime = trackDuration * value;
		var minutesLeft = timeLeft/60; 
		var secondesLeft = timeLeft%60; // modulo to get seconde...
		var actualMinutes = actualTime/60; 
		var actualSecondes = actualTime%60; // modulo to get seconde...
		
		if (engine.getValue(group, "track_loaded")) {
			midi.sendShortMsg(0x91,0x44,0x7F); // For remaining time : "-"
			midi.sendShortMsg(0x91,0x42,minutesLeft); // send minutes
			midi.sendShortMsg(0x91,0x43,secondesLeft); // send secondes
			PioneerDDJ1000.CurrentPositionBarUpdate(2,actualSecondes); // Update Position Bar for channel 2
		} else {
			midi.sendShortMsg(0x91,0x42,0); // clear minutes
			midi.sendShortMsg(0x91,0x43,0); // clear secondes
		}
	};
	
	PioneerDDJ1000.CurrentPositionBarUpdate = function (channelNumber,value){ // David Goodenough
		var currentBarPosition = value*6*positionBarSpeedFactor;
		var channelNbrCode = 176+(channelNumber-1);
		var valueOnA360Circle = currentBarPosition%360;
		var MSB = valueOnA360Circle/128;
		var LSB = 0;
		
		if (MSB < 1) {
			MSB = 0;
		} else {
				if (MSB < 2) {
					MSB = 1;
				} else {
						if (MSB < 3) {
							MSB = 2;
						}
				}
		}
		
		LSB = valueOnA360Circle - (128*MSB);
		
		midi.sendShortMsg(channelNbrCode,0x14,MSB);
		midi.sendShortMsg(channelNbrCode,0x34,LSB);
		
		
	};
	
	PioneerDDJ1000.BpmOnJogCH1 = function (value, group, control) {  // For Track BPM and Pitch range
		
		var absoluteBPM = Math.floor(value*10);
		var absolutePitchRange = Math.floor(((actualTempoCH1 * 10) * engine.getValue(group, "rate")) * 100);
		var adaptPitchRange = 0;
		var MSB = 0;
		var LSB = 0;
		
		// Bpm value update
		
		if (value == 0) {
			MSB = 0;
			LSB = 0;
		} else {
			MSB = Math.floor(absoluteBPM / 128);
			LSB = absoluteBPM % 128;
		}
		
		midi.sendShortMsg(0xB0,0x15,MSB);
		midi.sendShortMsg(0xB0,0x35,LSB);
		
		// Pitch Range update
		
		
		
		if (absolutePitchRange < 0) {
			adaptPitchRange = 1000 - absolutePitchRange;
			MSB = Math.floor(adaptPitchRange / 128);
			LSB = adaptPitchRange % 128;
		} else {
			adaptPitchRange = 1000 + absolutePitchRange;
			MSB = Math.floor(adaptPitchRange / 128);
			LSB = adaptPitchRange % 128;
		}
		
		midi.sendShortMsg(0xB0,0x16,MSB);
		midi.sendShortMsg(0xB0,0x36,LSB);

	};
	
	PioneerDDJ1000.BpmOnJogCH2 = function (value, group, control) {  // For Track BPM and Pitch range
		
		var absoluteBPM = Math.floor(value*10);
		var absolutePitchRange = Math.floor(((actualTempoCH2 * 10) * engine.getValue(group, "rate")) * 100);
		var adaptPitchRange = 0;
		var MSB = 0;
		var LSB = 0;
		
		// Bpm value update
		
		if (value == 0) {
			MSB = 0;
			LSB = 0;
		} else {
			MSB = Math.floor(absoluteBPM / 128);
			LSB = absoluteBPM % 128;
		}
		
		midi.sendShortMsg(0xB1,0x15,MSB);
		midi.sendShortMsg(0xB1,0x35,LSB);
		
		// Pitch Range update
		
		
		
		if (absolutePitchRange < 0) {
			adaptPitchRange = 1000 - absolutePitchRange;
			MSB = Math.floor(adaptPitchRange / 128);
			LSB = adaptPitchRange % 128;
		} else {
			adaptPitchRange = 1000 + absolutePitchRange;
			MSB = Math.floor(adaptPitchRange / 128);
			LSB = adaptPitchRange % 128;
		}
		
		midi.sendShortMsg(0xB1,0x16,MSB);
		midi.sendShortMsg(0xB1,0x36,LSB);

	};
	
// ---------------------------------------------------------------------------------------------------------------
// ------------------------- PreFader Level ----------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------

	PioneerDDJ1000.LedVuMeterCH1 = function (value, group, control){  // GOD style...
		var newValue = value*127;
		midi.sendShortMsg(0xB0,0x02,newValue);
	};
	
	PioneerDDJ1000.LedVuMeterCH2 = function (value, group, control){  // GOD style...
		var newValue = value*127;
		midi.sendShortMsg(0xB1,0x02,newValue);
	};
	

// ---------------------------------------------------------------------------------------------------------------
// ------------------------- Loop LED and Behevior ---------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------


// ---------------------------------------------------------------------------------------------------------------
// ------------------------- HotCues -----------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------
	PioneerDDJ1000.HotCuesUpdate1CH1 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x97,0x00,hotCueColor);
		} else {
			midi.sendShortMsg(0x97,0x00,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate1CH2 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x99,0x00,hotCueColor);
		} else {
			midi.sendShortMsg(0x99,0x00,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate2CH1 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x97,0x01,hotCueColor);
		} else {
			midi.sendShortMsg(0x97,0x01,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate2CH2 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x99,0x01,hotCueColor);
		} else {
			midi.sendShortMsg(0x99,0x01,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate3CH1 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x97,0x02,hotCueColor);
		} else {
			midi.sendShortMsg(0x97,0x02,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate3CH2 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x99,0x02,hotCueColor);
		} else {
			midi.sendShortMsg(0x99,0x02,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate4CH1 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x97,0x03,hotCueColor);
		} else {
			midi.sendShortMsg(0x97,0x03,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate4CH2 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x99,0x03,hotCueColor);
		} else {
			midi.sendShortMsg(0x99,0x03,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate5CH1 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x97,0x04,hotCueColor);
		} else {
			midi.sendShortMsg(0x97,0x04,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate5CH2 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x99,0x04,hotCueColor);
		} else {
			midi.sendShortMsg(0x99,0x04,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate6CH1 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x97,0x05,hotCueColor);
		} else {
			midi.sendShortMsg(0x97,0x05,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate6CH2 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x99,0x05,hotCueColor);
		} else {
			midi.sendShortMsg(0x99,0x05,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate7CH1 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x97,0x06,hotCueColor);
		} else {
			midi.sendShortMsg(0x97,0x06,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate7CH2 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x99,0x06,hotCueColor);
		} else {
			midi.sendShortMsg(0x99,0x06,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate8CH1 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x97,0x07,hotCueColor);
		} else {
			midi.sendShortMsg(0x97,0x07,black);
		}
	};
	
	PioneerDDJ1000.HotCuesUpdate8CH2 = function (value, group, control){
		if (value == 1) {
			midi.sendShortMsg(0x99,0x07,hotCueColor);
		} else {
			midi.sendShortMsg(0x99,0x07,black);
		}
	};

// ---------------------------------------------------------------------------------------------------------------
// ------------------------- Tempo Change ------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------

	PioneerDDJ1000.MaxTempoChangeCH1 = function (channel, control, value, status, group) {
		
		if (value == 0x7F) {
			switch (actualTempoValueCH1) {
				case 0: engine.setValue("[Channel1]", "rateRange", Tempo12); actualTempoCH1 = Tempo12; break;
				case 1: engine.setValue("[Channel1]", "rateRange", Tempo24); actualTempoCH1 = Tempo24; break;
				case 2: engine.setValue("[Channel1]", "rateRange", Tempo100); actualTempoCH1 = Tempo100; break;
				case 3: engine.setValue("[Channel1]", "rateRange", Tempo8); actualTempoCH1 = Tempo8; break;
			}
			
			switch (actualTempoValueCH1) {
				case 0: actualTempoValueCH1 = actualTempoValueCH1 + 1; break;
				case 1: actualTempoValueCH1 = actualTempoValueCH1 + 1; break;
				case 2: actualTempoValueCH1 = actualTempoValueCH1 + 1; break;
				case 3: actualTempoValueCH1 = 0;
			}
		}
	};
	
	PioneerDDJ1000.MaxTempoChangeCH2 = function (channel, control, value, status, group) {
		
		if (value == 0x7F) {
			switch (actualTempoValueCH2) {
				case 0: engine.setValue("[Channel2]", "rateRange", Tempo12); actualTempoCH2 = Tempo12; break;
				case 1: engine.setValue("[Channel2]", "rateRange", Tempo24); actualTempoCH2 = Tempo24; break;
				case 2: engine.setValue("[Channel2]", "rateRange", Tempo100); actualTempoCH2 = Tempo100; break;
				case 3: engine.setValue("[Channel2]", "rateRange", Tempo8); actualTempoCH2 = Tempo8; break;
			}
			
			switch (actualTempoValueCH2) {
				case 0: actualTempoValueCH2 = actualTempoValueCH2 + 1; break;
				case 1: actualTempoValueCH2 = actualTempoValueCH2 + 1; break;
				case 2: actualTempoValueCH2 = actualTempoValueCH2 + 1; break;
				case 3: actualTempoValueCH2 = 0;
			}
		}
		
	};
	
// ---------------------------------------------------------------------------------------------------------------
// ------------------------- FX ----------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------

	PioneerDDJ1000.FxChainSelectorNext = function (channel, control, value, status, group) {
		if (value == 0x7F) { // If pressed only, side effect with release
			engine.setValue("[EffectRack1_EffectUnit1_Effect1]", "next_effect", 1);
		}
	};
	
	PioneerDDJ1000.FxChainSelectorPrevious = function (channel, control, value, status, group) {
		if (value == 0x7F) { // If pressed only, side effect with release
			engine.setValue("[EffectRack1_EffectUnit1_Effect1]", "prev_chain", 1);
		}
	};
