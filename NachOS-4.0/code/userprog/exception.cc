// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MaxFileLength 32
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------
void IncreasePC()
{
	int counter = kernel->machine->ReadRegister(PCReg);
   	kernel->machine->WriteRegister(PrevPCReg, counter);
    counter = kernel->machine->ReadRegister(NextPCReg);
    kernel->machine->WriteRegister(PCReg, counter);
   	kernel->machine->WriteRegister(NextPCReg, counter + 4);
}


char *User2System(int virtAddr, int limit)
{
	int i; // index
	int oneChar;
	char *kernelBuf = NULL;
	kernelBuf = new char[limit + 1];
	if (kernelBuf == NULL)
		return kernelBuf;
	memset(kernelBuf, 0, limit + 1);
	for (i = 0; i < limit; i++)
	{
		kernel->machine->ReadMem(virtAddr + i, 1, &oneChar);
		kernelBuf[i] = (char)oneChar;
		if (oneChar == 0)
			break;
	}
	return kernelBuf;
}

int System2User(int virtAddr, int len, char *buffer)
{
	if (len < 0)
		return -1;
	if (len == 0)
		return len;
	int i = 0;
	int oneChar = 0;
	do
	{
		oneChar = (int)buffer[i];
		kernel->machine->WriteMem(virtAddr + i, 1, oneChar);
		i++;
	} while (i < len && oneChar != 0);
	return i;
}
void ExceptionHandler(ExceptionType which)
{
	int type = kernel->machine->ReadRegister(2);

	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

	switch (which)
	{
	case SyscallException:
		switch (type)
		{
		case SC_Halt:
			DEBUG(dbgSys, "Shutdown, initiated by user program.\n");
			SysHalt();
			ASSERTNOTREACHED();
			break;
		case SC_Create:
	{
		int virtAddr;
		char* filename = NULL;

		virtAddr = kernel->machine->ReadRegister(4);

		filename = User2System(virtAddr, MaxFileLength + 1);
		if (filename == NULL){
			printf("\n Not enough memory in system");
			kernel->machine->WriteRegister(2, -1);

			delete[] filename;
			return;
		}

		if (kernel->fileSystem->Create(filename) == 0){
			kernel->machine->WriteRegister(2, -1);

			delete[] filename;
			return;
		}
		kernel->machine->WriteRegister(2, 0);
		
		delete[] filename;
		break;
	}


		case SC_Open:
		{
			// Input: arg1: Dia chi cua chuoi name, arg2: type
			// Output: Tra ve OpenFileID neu thanh, -1 neu loi
			// Chuc nang: Tra ve ID cua file.

			// OpenFileID Open(char *name, int type)
			int virtAddr = kernel->machine->ReadRegister(4); // Lay dia chi cua tham so name tu thanh ghi so 4
			int type = kernel->machine->ReadRegister(5);	 // Lay tham so type tu thanh ghi so 5
			char *filename;
			filename = User2System(virtAddr, 32); // Copy chuoi tu vung nho User Space sang System Space voi bo dem name dai MaxFileLength
			// Kiem tra xem OS con mo dc file khong

			// update 4/1/2018
			int freeSlot = kernel->fileSystem->FindFreeSlot();
			if (freeSlot != -1) // Chi xu li khi con slot trong
			{
				DEBUG(dbgSys, "Free Slot OKE\n");
				if (type == 0 || type == 1) // chi xu li khi type = 0 hoac 1
				{

					if ((kernel->fileSystem->openf[freeSlot] = kernel->fileSystem->Open(filename, type)) != NULL) // Mo file thanh cong
					{
						DEBUG(dbgSys, "Type 0-1\n");

						kernel->machine->WriteRegister(2, freeSlot); // tra ve OpenFileID
					}
				}
				else if (type == 2) // xu li stdin voi type quy uoc la 2
				{
					DEBUG(dbgSys, "Type 2\n");
					kernel->machine->WriteRegister(2, 0); // tra ve OpenFileID
				}
				else // xu li stdout voi type quy uoc la 3
				{
					DEBUG(dbgSys, "Type 3\n");
					kernel->machine->WriteRegister(2, 1); // tra ve OpenFileID
				}
				delete[] filename;
				IncreasePC();
				return;
			}
			kernel->machine->WriteRegister(2, -1); // Khong mo duoc file return -1

			delete[] filename;
			IncreasePC();
			return;
			ASSERTNOTREACHED();
		}
		break;
		case SC_Close:
		{
			//Input id cua file(OpenFileID)
			// Output: 0: thanh cong, -1 that bai
			int fid = kernel->machine->ReadRegister(4); // Lay id cua file tu thanh ghi so 4
			if (fid >= 0 && fid <= 14) //Chi xu li khi fid nam trong [0, 14]
			{
				if (kernel->fileSystem->openf[fid]) //neu mo file thanh cong
				{
					delete kernel->fileSystem->openf[fid]; //Xoa vung nho luu tru file
					kernel->fileSystem->openf[fid] = NULL; //Gan vung nho NULL
					kernel->machine->WriteRegister(2, 0);
				}
				DEBUG(dbgSys, "Close success\n");
				IncreasePC();
				return;
			}
			DEBUG(dbgSys, "Close failed\n");
			kernel->machine->WriteRegister(2, -1);
			IncreasePC();
			return;
			ASSERTNOTREACHED();	
		}
		break;
		// case SC_Read:
		// {
		// 	// Input: buffer(char*), so ky tu(int), id cua file(OpenFileID)
		// 	// Output: -1: Loi, So byte read thuc su: Thanh cong, -2: Thanh cong
		// 	// Cong dung: Doc file voi tham so la buffer, so ky tu cho phep va id cua file
		// 	int virtAddr = kernel->machine->ReadRegister(4); // Lay dia chi cua tham so buffer tu thanh ghi so 4
		// 	int charcount = kernel->machine->ReadRegister(5); // Lay charcount tu thanh ghi so 5
		// 	int id = kernel->machine->ReadRegister(6); // Lay id cua file tu thanh ghi so 6 
		// 	int OldPos;
		// 	int NewPos;
		// 	char *buf;
		// 	// Kiem tra id cua file truyen vao co nam ngoai bang mo ta file khong
		// 	if (id < 0 || id > 14)
		// 	{
		// 		printf("\nKhong the read vi id nam ngoai bang mo ta file.");
		// 		kernel->machine->WriteRegister(2, -1);
		// 		IncreasePC();
		// 		return;
		// 	}
		// 	// Kiem tra file co ton tai khong
		// 	if (kernel->fileSystem->openf[id] == NULL)
		// 	{
		// 		printf("\nKhong the read vi file nay khong ton tai.");
		// 		kernel->machine->WriteRegister(2, -1);
		// 		IncreasePC();
		// 		return;
		// 	}
		// 	if (kernel->fileSystem->openf[id]->type == 3) // Xet truong hop doc file stdout (type quy uoc la 3) thi tra ve -1
		// 	{
		// 		printf("\nKhong the read file stdout.");
		// 		kernel->machine->WriteRegister(2, -1);
		// 		IncreasePC();
		// 		return;
		// 	}
		// 	OldPos = kernel->fileSystem->openf[id]->GetCurrentPos(); // Kiem tra thanh cong thi lay vi tri OldPos
		// 	buf = User2System(virtAddr, charcount); // Copy chuoi tu vung nho User Space sang System Space voi bo dem buffer dai charcount
		// 	// Xet truong hop doc file stdin (type quy uoc la 2)
		// 	if (kernel->fileSystem->openf[id]->type == 2)
		// 	{
		// 		// Su dung ham Read cua lop SynchConsole de tra ve so byte thuc su doc duoc
		// 		int size = gSynchConsole->Read(buf, charcount); 
		// 		System2User(virtAddr, size, buf); // Copy chuoi tu vung nho System Space sang User Space voi bo dem buffer co do dai la so byte thuc su
		// 		kernel->machine->WriteRegister(2, size); // Tra ve so byte thuc su doc duoc
		// 		delete buf;
		// 		IncreasePC();
		// 		return;
		// 	}
		// 	// Xet truong hop doc file binh thuong thi tra ve so byte thuc su
		// 	if ((kernel->fileSystem->openf[id]->Read(buf, charcount)) > 0)
		// 	{
		// 		// So byte thuc su = NewPos - OldPos
		// 		NewPos = kernel->fileSystem->openf[id]->GetCurrentPos();
		// 		// Copy chuoi tu vung nho System Space sang User Space voi bo dem buffer co do dai la so byte thuc su 
		// 		System2User(virtAddr, NewPos - OldPos, buf); 
		// 		kernel->machine->WriteRegister(2, NewPos - OldPos);
		// 	}
		// 	else
		// 	{
		// 		// Truong hop con lai la doc file co noi dung la NULL tra ve -2
		// 		//printf("\nDoc file rong.");
		// 		kernel->machine->WriteRegister(2, -2);
		// 	}
		// 	delete buf;
		// 	IncreasePC();
		// 	return;
		// }
			
		case SC_Socket:
		{
			if (socket(AF_INET, SOCK_STREAM, 0) < 0)
			{
				DEBUG(dbgSys, "Create Socket failed\n");
			}
			else{
				DEBUG(dbgSys, "Create Socket success\n");
			}
			IncreasePC();
			return;
			ASSERTNOTREACHED();
		}
		break;
		// case SC_Connect:
		// {
		// 	struct sockaddr_in serv_addr;
		// 	memset(&serv_addr, '0', sizeof(serv_addr));
		// 	serv_addr.sin_family = AF_INET;
		// 	serv_addr.sin_port = htons(5000);
		// 	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr) < 0)
		// 	{
		// 		DEBUG(dbgSys, "Invalid address\n");
		// 	}
		// 	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		// 	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		// 	{
		// 		DEBUG(dbgSys, "Connect failed\n");
		// 	}
		// }
		case SC_Add:
		{
			DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");

			/* Process SysAdd Systemcall*/
			int result;
			result = SysAdd(/* int op1 */ (int)kernel->machine->ReadRegister(4),
							/* int op2 */ (int)kernel->machine->ReadRegister(5));

			DEBUG(dbgSys, "Add returning with " << result << "\n");
			/* Prepare Result */
			kernel->machine->WriteRegister(2, (int)result);

			/* Modify return point */
			{
				/* set previous programm counter (debugging only)*/
				kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

				/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
				kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

				/* set next programm counter for brach execution */
				kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}

			return;

			ASSERTNOTREACHED();
		}
		break;

		default:
			cerr << "Unexpected system call " << type << "\n";
			break;
		}
		break;
	default:
		cerr << "Unexpected user mode exception" << (int)which << "\n";
		break;
	}
	ASSERTNOTREACHED();
}
