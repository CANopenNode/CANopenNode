
// Includes 
#include "kinetis.h"
extern "C" {				// CANOpenNode stack is written in C so we have to watch out for name mangling linker errors
	#include "CO_driver.h"
	#include "CO_OD.h"
	#include "CANopen.h"
}

// Expose a C linked debug function for use in the CO driver (only needed if you want to enable debugging - see #define in co_driver.c)
extern "C" void debug(const char *format, ...);


void setup() {

	// Serial
	Serial.begin(115200);
	while(!Serial);
	delay(100);

    // Set up the  CANBus pins and clock source (Lifted from FlexCAN)
    CORE_PIN3_CONFIG = PORT_PCR_MUX(2);
    CORE_PIN4_CONFIG = PORT_PCR_MUX(2);// | PORT_PCR_PE | PORT_PCR_PS;
    OSC0_CR |= OSC_ERCLKEN;
    SIM_SCGC6 |=  SIM_SCGC6_FLEXCAN0;

	// Init CANOpen with node ID 4
	CO_ReturnError_t err=CO_init(0,0x4,500);

	if(err) {
		Serial.print("Error initializing CANOpen: ");
		Serial.print(err, HEX);
		Serial.print("\n");
		while(1);
	}

	// Attach CANopen interrupt (handles incoming messages etc)
	attachInterruptVector(IRQ_CAN_MESSAGE, can_message);
	NVIC_ENABLE_IRQ(IRQ_CAN_MESSAGE);
	sei();

	// Enable CAN and wait unit ready
	CO_CANsetNormalMode(CO->CANmodule[0]);

	// Set up CANopen tick timer at 1ms interval
	canopen_timer.priority(1);
	canopen_timer.begin(canopen_tick, 1000);	

	// (optional) configure a callback when OD item 0x2001 is changed
	CO_OD_configure(CO->SDO[0],0x2001, &on_od_request, NULL, 0 ,0);

}



/**
 * Object Dictionary callback
 *
 * We received a command to change something in the OD.
 * This callback fires for OD items that we configured with CO_OD_configure.
 * We wield a lot of power here, for details and more info on ODF_arg, see CO_ODF_arg_t in CO_SDO.h
 * 
 * @param {CO_ODF_arg_t*} ODF_arg data on what is changing
 * @return {CO_SDO_abortCode_t} abort code (normally CO_SDO_AB_NONE but see CO_SDO_abordCode_t in CO_SDO.h) 
 */
CO_SDO_abortCode_t on_od_request(CO_ODF_arg_t *ODF_arg) {

	switch(ODF_arg->index) {	
	
	case 0x2001:
		// Do something
		return CO_SDO_AB_NONE;
		break;
	}

	// Default: return an error and abort the operation
	return CO_SDO_AB_UNSUPPORTED_ACCESS;
}


void loop() {

	// Blink the LED, whatever

	delay(1000);

}


/**
 * CAN interrupt handler
 *
 * @private
 */
void can_message(){

	CO_CANinterrupt(CO->CANmodule[0]);
}

/**
 * CAN tick interrupt function
 *
 * @private
 */
void canopen_tick() {

	CO_process(CO,canopen_elapsed, NULL);
	canopen_elapsed=0;

}


/**
 * Debug
 * 
 * Expose a C linked debug function for use in the CO driver.  We rrite to the serial console
 * (we have to wrap Serial.printf because linked C code can't call objects).
 *
 * @private
 * @param {const char*} format
 * @param {type} parameters ...
 */
void debug(const char *format, ...){

	// Some place to put the string
	static char buffer[400];

	// Write the string to the buffer
	int len;
	va_list args;
	va_start(args, format);
	len=vsprintf(buffer, format, args);
	va_end(args);

	// Send the buffer to Serial
	Serial.write(buffer, len);
}
