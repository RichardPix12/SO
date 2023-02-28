#include <stdio.h>
#include <stdlib.h>
#include "ComputerSystem.h"
#include "OperatingSystem.h"
#include "ComputerSystemBase.h"
#include "Processor.h"
#include "Messages.h"
#include "Asserts.h"
#include "Wrappers.h"

// Functions prototypes
void ComputerSystem_PrintProgramList();
// Powers on of the Computer System.
void ComputerSystem_PowerOn(int argc, char *argv[], int paramIndex) {
	
	// Obtain a list of programs in the command line
	int daemonsBaseIndex = ComputerSystem_ObtainProgramList(argc, argv, paramIndex);
	
	// Load debug messages
	int nm=0;
	nm=Messages_Load_Messages(nm,TEACHER_MESSAGES_FILE);
	if (nm<0) {
		ComputerSystem_DebugMessage(64,SHUTDOWN,TEACHER_MESSAGES_FILE);
		exit(2);
	}
	nm=Messages_Load_Messages(nm,STUDENT_MESSAGES_FILE);

	// Prepare if necesary the assert system
	
	Asserts_LoadAsserts();
	ComputerSystem_PrintProgramList();
	//ComputerSystem_PrintProgramList();
	// Request the OS to do the initial set of tasks. The last one will be
	// the processor allocation to the process with the highest priority
	OperatingSystem_Initialize(daemonsBaseIndex);
	
	// Tell the processor to begin its instruction cycle 
	Processor_InstructionCycleLoop();
	
}

// Powers off the CS (the C program ends)
void ComputerSystem_PowerOff() {
	// Show message in red colour: "END of the simulation\n" 
	ComputerSystem_DebugMessage(99,SHUTDOWN,"END of the simulation\n"); 
	exit(0);
}

/////////////////////////////////////////////////////////
//  New functions below this line  //////////////////////

// funcion  que muestre en pantalla los
// programas de usuario contenidos en el vector programsList

void ComputerSystem_PrintProgramList(){
	//Lanzamos el inicio
	ComputerSystem_DebugMessage(101,INIT);
	//Iniciamos el bucle para todos los demas programas
	int i;
	for(i=1; i<PROGRAMSMAXNUMBER; i++){// En 1 porque el primero es reservado
		if(programList[i] > 0x0){
			//progData = programList[i];
			ComputerSystem_DebugMessage(102,INIT,programList[i]->executableName,programList[i]->arrivalTime );
		}
	}
	
}