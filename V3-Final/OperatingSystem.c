#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
// Functions prototypes
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_HandleClockInterrupt();
void OperatingSystem_BlockProcess();
void OperatingSystem_ExtractFromBlockedToRun();
int OperatingSystem_RemoveSleepingProcess();
int OperatingSystem_GetExecutingProcessID();
//Ejercicio 10 estados
char * statesNames [5]={"NEW","READY","EXECUTING","BLOCKED","EXIT"};

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID=NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation
int initialPID=(sizeof(processTable)/sizeof(PCB))-1;

// Begin indes for daemons in programList
int baseDaemonsInProgramList; 

// Array that contains the identifiers of the READY processes

// Antes del 11
//heapItem readyToRunQueue[PROCESSTABLEMAXSIZE];
//int numberOfReadyToRunProcesses=0;

//Despues del 11
heapItem readyToRunQueue [NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES]={0,0};
char * queueNames [NUMBEROFQUEUES]={"USER","DAEMONS"};

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

int numberOfClockInterrupts = 0;

heapItem sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses=0;

char DAEMONS_PROGRAMS_FILE[MAXIMUMLENGTH]="teachersDaemons";

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code
	programFile=fopen("OperatingSystemCode", "r");
	if (programFile==NULL){
		// Show red message "FATAL ERROR: Missing Operating System!\n"
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing Operating System!\n");
		exit(1);		
	}

	// Obtain the memory requirements of the program
	int processSize=OperatingSystem_ObtainProgramSize(programFile);

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);
	
	// Process table initialization (all entries are free)
	for (i=0; i<PROCESSTABLEMAXSIZE;i++){
		processTable[i].busy=0;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+2);
		
	// Include in program list  all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);
	
	// Create all user processes from the information given in the command line
	
	ComputerSystem_FillInArrivalTimeQueue();			// Ejercicio 0 V3

	OperatingSystem_PrintStatus();						// Ejercicio 0 V3
	
	if(OperatingSystem_LongTermScheduler()==0){
		OperatingSystem_ReadyToShutdown();
	}
	
	if (strcmp(programList[processTable[sipID].programListIndex]->executableName,"SystemIdleProcess")) {
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing SIP program!\n");
		exit(1);		
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command line and daemons programs
int OperatingSystem_LongTermScheduler() {
  
	int PID, i,
		numberOfSuccessfullyCreatedProcesses=0;
	while (OperatingSystem_IsThereANewProgram() == 1){
			i = Heap_getFirst(arrivalTimeQueue, numberOfProgramsInArrivalTimeQueue); // devuelve indice del programa el programList
			if (programList[i]->arrivalTime <= Clock_GetTime()){
				PID = OperatingSystem_CreateProcess(i);
			switch(PID){
				case NOFREEENTRY:
					OperatingSystem_ShowTime(ERROR);
					ComputerSystem_DebugMessage(103,ERROR,programList[i] -> executableName);
					Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
					break;
				case PROGRAMDOESNOTEXIST:
					OperatingSystem_ShowTime(ERROR);
					ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, " it does not exist ");
					Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
					break;
				case PROGRAMNOTVALID:
					OperatingSystem_ShowTime(ERROR);
					ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, " invalid priority or size ");
					Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
					break;
				case  TOOBIGPROCESS:
					OperatingSystem_ShowTime(ERROR);
					ComputerSystem_DebugMessage(105, ERROR, programList[i]->executableName);
					Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
					break;
				
				default:
				numberOfSuccessfullyCreatedProcesses++;
				if (programList[i]->type==USERPROGRAM){ 
					numberOfNotTerminatedUserProcesses++;
					
				}
				Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
				// Move process to the ready state
				OperatingSystem_MoveToTheREADYState(PID);
				//OperatingSystem_PrintStatus();
			}
		}
		if (numberOfSuccessfullyCreatedProcesses > 0){
		OperatingSystem_ExtractFromBlockedToRun();		
		OperatingSystem_PrintStatus();		
		}
	}
	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}
int OperatingSystem_GetExecutingProcessID(){
	return executingProcessID;
}

// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {
  
	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram=programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID=OperatingSystem_ObtainAnEntryInTheProcessTable();
	
	if(PID == NOFREEENTRY){
		return NOFREEENTRY;
	}

	// Check if programFile exists
	programFile=fopen(executableProgram->executableName, "r");
	
	if( programFile == NULL){
		return PROGRAMDOESNOTEXIST;
	}
	

	// Obtain the memory requirements of the program
	processSize=OperatingSystem_ObtainProgramSize(programFile);	
	
	if (processSize == PROGRAMNOTVALID){
		return PROGRAMNOTVALID;
	}

	// Obtain the priority for the process
	priority=OperatingSystem_ObtainPriority(programFile);
	
	if(priority == PROGRAMNOTVALID){
		return PROGRAMNOTVALID;
	}
	// Obtain enough memory space
 	loadingPhysicalAddress=OperatingSystem_ObtainMainMemory(processSize, PID);
	
	if(loadingPhysicalAddress == TOOBIGPROCESS){
		return TOOBIGPROCESS;
	}

	// Load program in the allocated memory
	int loadProgram = OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize);
	if(loadProgram == TOOBIGPROCESS){
		return TOOBIGPROCESS;
	}
	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);
	
	// Show message "Process [PID] created from program [executableName]\n"
	OperatingSystem_ShowTime(INIT);
	ComputerSystem_DebugMessage(70,INIT,PID,executableProgram->executableName);
	
	return PID;
}


// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID) {

 	if (processSize>MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;
	
 	return PID*MAINMEMORYSECTIONSIZE;
}


// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex) {

	processTable[PID].busy=1;
	processTable[PID].initialPhysicalAddress=initialPhysicalAddress;
	processTable[PID].processSize=processSize;
	processTable[PID].state=NEW;
	processTable[PID].priority=priority;
	processTable[PID].programListIndex=processPLIndex;
	processTable[PID].copyOfAccumulatorRegister=0;
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(111, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[NEW]);
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {
		processTable[PID].copyOfPCRegister=initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister= ((unsigned int) 1) << EXECUTION_MODE_BIT;
		processTable[PID].queueID = DAEMONSQUEUE;
	} 
	else {
		processTable[PID].copyOfPCRegister=0;
		processTable[PID].copyOfPSWRegister=0;
		processTable[PID].queueID = USERPROCESSQUEUE;
	}
	
}

// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID) {
	
	if (Heap_add(PID, readyToRunQueue[processTable[PID].queueID],QUEUE_PRIORITY ,
		&numberOfReadyToRunProcesses[processTable[PID].queueID] ,PROCESSTABLEMAXSIZE)>=0) {
		processTable[PID].state=READY;
	

		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110,SYSPROC,PID,programList[processTable[PID].programListIndex]->executableName,statesNames[NEW],statesNames[READY]);
	} 
	
	//OperatingSystem_PrintReadyToRunQueue();
}


// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler() {
	
	int selectedProcess;

	selectedProcess=OperatingSystem_ExtractFromReadyToRun();
	
	return selectedProcess;
}


// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun() {
  
	int selectedProcess=NOPROCESS;
	for(int i = 0; i < NUMBEROFQUEUES; i++){
		selectedProcess=Heap_poll(readyToRunQueue[i],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[i]);
		if(selectedProcess!=NOPROCESS){
			return selectedProcess;
		}
	}
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}


// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {

	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	// Change the process' state
	processTable[executingProcessID].state=EXECUTING;
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[READY],statesNames[EXECUTING]);
	
	OperatingSystem_RestoreContext(PID);
}


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
  
	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE-1,processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-2,processTable[PID].copyOfPSWRegister);
	
	Processor_CopyInSystemStack(MAINMEMORYSIZE-3,processTable[PID].copyOfAccumulatorRegister);
	
	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}


// Function invoked when the executing process leaves the CPU 
void OperatingSystem_PreemptRunningProcess() {

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}


// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID) {
	
	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-1);
	
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-2);
	
	// Load Accumulator
	processTable[PID].copyOfAccumulatorRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-3);
}


// Exception management routine
void OperatingSystem_HandleException() {
  
	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(71,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
	
	OperatingSystem_TerminateProcess();
	OperatingSystem_PrintStatus();
}


// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {
  
	int selectedProcess;
  	
	processTable[executingProcessID].state=EXIT;	
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[EXECUTING],statesNames[EXIT]);
	
	if (programList[processTable[executingProcessID].programListIndex]->type==USERPROGRAM) 
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	
	if (numberOfNotTerminatedUserProcesses==0) {
		if (executingProcessID==sipID) {
			// finishing sipID, change PC to address of OS HALT instruction
			OperatingSystem_TerminatingSIP();
			ComputerSystem_DebugMessage(99,SHUTDOWN,"The system will shut down now...\n");
			return; // Don't dispatch any process
		}
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall() {
  
	int systemCallID;
	int PID;
	// Register A contains the identifier of the issued system call
	systemCallID=Processor_GetRegisterA();
	
	switch (systemCallID) {
		case SYSCALL_PRINTEXECPID:
			// Show message: "Process [executingProcessID] has the processor assigned\n"
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(72,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			break;

		case SYSCALL_END:
			// Show message: "Process [executingProcessID] has requested to terminate\n"
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(73,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_TerminateProcess();
			OperatingSystem_PrintStatus();
			break;
		
		case SYSCALL_YIELD:
			
			PID = OperatingSystem_ExtractFromReadyToRun();
			if (processTable[PID].priority == processTable[executingProcessID].priority && PID != NOPROCESS) {
				OperatingSystem_ShowTime(SYSPROC);
				ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, executingProcessID,
					programList[processTable[executingProcessID].programListIndex]->executableName,
					PID, programList[processTable[PID].programListIndex]->executableName);
				OperatingSystem_PreemptRunningProcess();	// Quitamos el proceso que está en ejecución
				OperatingSystem_Dispatch(PID);				// Elegimos el otro proceso como el que se va a ejecutar
				OperatingSystem_PrintStatus();
			}
			break;
		
		case SYSCALL_SLEEP:
			OperatingSystem_SaveContext(executingProcessID);
			OperatingSystem_BlockProcess();
			OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
			OperatingSystem_PrintStatus();
			
			break;
			
	}
}

void OperatingSystem_BlockProcess(){
	int num = fabs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1;
	
	processTable[executingProcessID].whenToWakeUp = num;
	OperatingSystem_SaveContext(executingProcessID);
	if(Heap_add(executingProcessID,sleepingProcessesQueue,QUEUE_WAKEUP,&numberOfSleepingProcesses, PROCESSTABLEMAXSIZE ) >= 0){
		processTable[executingProcessID].state = BLOCKED;
	}
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[EXECUTING],statesNames[BLOCKED]);
	executingProcessID = NOPROCESS;
}

	
//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint){
	switch (entryPoint){
		case SYSCALL_BIT: // SYSCALL_BIT=2
			OperatingSystem_HandleSystemCall();
			break;
		case EXCEPTION_BIT: // EXCEPTION_BIT=6
			OperatingSystem_HandleException();
			break;
		case CLOCKINT_BIT: // CLOCKINT_BIT=9
			OperatingSystem_HandleClockInterrupt();
			break;
	}

}


void OperatingSystem_HandleClockInterrupt(){ 
	numberOfClockInterrupts++;

	OperatingSystem_ShowTime(INTERRUPT);		// [tiempo]
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts); // Clock interrupt number [numberOfClockInterrupts] has occurred

	int selectedProcess = NOPROCESS;
	int procesosDesbloqueados = 0;

	//*********************************(APARTADO A)***************************************************************************************************************
	while (numberOfSleepingProcesses > 0 && processTable[sleepingProcessesQueue[0].info].whenToWakeUp == numberOfClockInterrupts) {
		// Cogemos uno de los procesos de la cola de dormidos
		selectedProcess = Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses);	
		// Movemos el proceso al estado READY y lo añadimos a la cola de listos
		OperatingSystem_MoveToTheREADYState(selectedProcess);	
		procesosDesbloqueados++;	// se desbloquea un proceso
	}

	//*********************************(APARTADO B)***************************************************************************************************************
	// Si se ha desbloqueado algún proceso, se mostrará el nuevo estado del sistema
	if(procesosDesbloqueados>0){
		
		OperatingSystem_RemoveSleepingProcess(procesosDesbloqueados);
	}
	
	
	OperatingSystem_LongTermScheduler();
	OperatingSystem_ExtractFromBlockedToRun();			// Ejercicio 6 V2
	
	return;

} 

void OperatingSystem_ExtractFromBlockedToRun(){
	int PID = NOPROCESS;
	//OperatingSystem_LongTermScheduler();
	// Si hay algún proceso de usuario
	if (numberOfReadyToRunProcesses[0] > 0)
		PID = Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE], numberOfReadyToRunProcesses[USERPROCESSQUEUE]);
	
	else
		PID = Heap_getFirst(readyToRunQueue[processTable[executingProcessID].queueID], 
				numberOfReadyToRunProcesses[processTable[executingProcessID].queueID]);
	
	if (PID == NOPROCESS)
		return;
	
	
	if (processTable[executingProcessID].queueID == DAEMONSQUEUE && processTable[PID].queueID == USERPROCESSQUEUE){
		// El proceso en ejecución ya no es el más prioritario y hay que cambiarlo por el nuevo proceso más prioritario
			int PIDEjecutando;
			PIDEjecutando = executingProcessID;

			//[27] Process [1 – prNam1] is thrown out of the processor by process [2 – prNam2] 
			OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
			ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, PIDEjecutando, 
				programList[processTable[PIDEjecutando].programListIndex]->executableName, PID, programList[processTable[PID].programListIndex]->executableName);

			OperatingSystem_PreemptRunningProcess();
			PID = OperatingSystem_ShortTermScheduler();
			OperatingSystem_Dispatch(PID);		// Asignamos el procesador al proceso más prioritario
				
			// Al cambiar el proceso en ejecución, llamamos a OperatingSystem_PrintStatus
			OperatingSystem_PrintStatus();
	}
	else{
		// Comprobamos si el proceso en ejecución sigue siendo el más prioritario
		if (processTable[executingProcessID].priority > processTable[PID].priority && PID != NOPROCESS) { 

			// El proceso en ejecución ya no es el más prioritario y hay que cambiarlo por el nuevo proceso más prioritario
			int PIDEjecutando;
			PIDEjecutando = executingProcessID;

			//[27] Process [1 – prNam1] is thrown out of the processor by process [2 – prNam2] 
			OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
			ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, PIDEjecutando, 
				programList[processTable[PIDEjecutando].programListIndex]->executableName, PID, programList[processTable[PID].programListIndex]->executableName);

			OperatingSystem_PreemptRunningProcess();
			PID = OperatingSystem_ShortTermScheduler();
			OperatingSystem_Dispatch(PID);		// Asignamos el procesador al proceso más prioritario
				
			// Al cambiar el proceso en ejecución, llamamos a OperatingSystem_PrintStatus
			OperatingSystem_PrintStatus();
		}
	}
}

int OperatingSystem_RemoveSleepingProcess(){
	return Heap_poll(sleepingProcessesQueue,QUEUE_WAKEUP ,&numberOfSleepingProcesses);
}

/* ANTES DEL 11+
void OperatingSystem_PrintReadyToRunQueue(){
	
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE); 
	for (int i = 0; i < numberOfReadyToRunProcesses; i++) {
		int PID = readyToRunQueue[i].info;
		if (i < numberOfReadyToRunProcesses-1)	
			ComputerSystem_DebugMessage(108,SHORTTERMSCHEDULE, PID, processTable[PID].priority);		
		else
			ComputerSystem_DebugMessage(107,SHORTTERMSCHEDULE, PID, processTable[PID].priority);		
	}
	
}
*/
void OperatingSystem_PrintReadyToRunQueue(){
	int i, j;
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE); // Ready-to-run processes queue: 
	for (i = 0; i < NUMBEROFQUEUES; i++) { // i < 2
		ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE, queueNames[i]); //	USERS: o DAEMONS:
			for (j = 0; j < numberOfReadyToRunProcesses[i]; j++) { // i < numProcesos de usuario o de demonio 
				if (j == numberOfReadyToRunProcesses[i] - 1) { // si estamos en la ultima posición de la cola de usuario o la de demonio
					ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, readyToRunQueue[i][j].info, processTable[readyToRunQueue[i][j].info].priority);	//\t[@G%d@@,%d]\n
				}
				else {
					ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, readyToRunQueue[i][j].info, processTable[readyToRunQueue[i][j].info].priority);	//\t[@G%d@@,%d],
				}
			}
	}
}

