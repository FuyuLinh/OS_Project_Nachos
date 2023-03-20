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
				kernel->machine->WriteRegister(2, -1);
				delete[] filename;
			}
			else if (kernel->fileSystem->Create(filename) == 0){
				kernel->machine->WriteRegister(2, -1);
				delete[] filename;
				IncreasePC();
				return;
			}
			kernel->machine->WriteRegister(2, 0);
			IncreasePC();
			delete[] filename;
			return;
		}
		case SC_Open:
		{
			int virtAddr = kernel->machine->ReadRegister(4); 
			int type = kernel->machine->ReadRegister(5);
			char *filename;
			filename = User2System(virtAddr, 32); 
			int freeSlot = kernel->fileSystem->FindFreeSlot();
			// find the slot free in file descriptor table to open the file
			if (freeSlot != -1) 
			{
				if (type == 0 || type == 1)
				{
					if ((kernel->fileSystem->openf[freeSlot] = kernel->fileSystem->Open(filename, type)) != NULL)
					{
						kernel->machine->WriteRegister(2, freeSlot);
					}
				}
				else if (type == 2 || type == 3)
				{
					// if stdin or stdout ... return 0 and 1
					kernel->machine->WriteRegister(2, type - 2);
				}
				else 
				{
					// can not open file
					kernel->machine->WriteRegister(2, -1); 
				}
				delete[] filename;
				IncreasePC();
				return;
			}
			// no have free slot
			kernel->machine->WriteRegister(2, -1);
			delete[] filename;
			IncreasePC();
			return;
		}
		case SC_Close:
		{
			
			int fid = kernel->machine->ReadRegister(4); 
			if (fid >= 0 && fid <= 14)
			{
				if (kernel->fileSystem->openf[fid]) 
				{
					delete kernel->fileSystem->openf[fid]; 
					// set the slot in file in file descriptor table = NULL
					kernel->fileSystem->openf[fid] = NULL; 
					kernel->machine->WriteRegister(2, 0);
				}
				IncreasePC();
				return;
			}
			kernel->machine->WriteRegister(2, -1);
			IncreasePC();
			return;
		}
		break;
		case SC_Read:
		{
			int virtAddr = kernel->machine->ReadRegister(4); 
			int charcount = kernel->machine->ReadRegister(5);
			int FileId = kernel->machine->ReadRegister(6);
			int startPos;
			int endPos;
			char *buf;
			// file descriptor table has only 20 slot
			if (FileId < 0 || FileId > 19)
			{
				DEBUG(dbgSys,"Here1");
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				return;
			}
			// check the file is exist
			if (kernel->fileSystem->openf[FileId] == NULL)
			{
				DEBUG(dbgSys,"File not exist");
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				return;
			}
			// stdout can not read
			if (kernel->fileSystem->openf[FileId]->type == 3) 
			{
				DEBUG(dbgSys,"Here3");
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				return;
			}
			startPos = kernel->fileSystem->openf[FileId]->GetCurrentPos(); 
			
			// stdin 
			if (kernel->fileSystem->openf[FileId]->type == 2)
			{
				buf = User2System(virtAddr, 255);
				// Returns the number of bytes read from the console
				int size = kernel->synchConsoleIn->Read(buf,255); 
				DEBUG(dbgSys,"Here2");
				System2User(virtAddr, size, buf); 
				kernel->machine->WriteRegister(2, size);
				delete buf;
				IncreasePC();
				return;
			}
			// normal file
			buf = User2System(virtAddr, charcount); 
			if ((kernel->fileSystem->openf[FileId]->Read(buf, charcount)) > 0)
			{
				DEBUG(dbgSys,"Here5");
				endPos = kernel->fileSystem->openf[FileId]->GetCurrentPos();
				// Returns the number of bytes read from the file
				System2User(virtAddr, endPos - startPos, buf); 
				kernel->machine->WriteRegister(2, endPos - startPos);
				DEBUG(dbgSys,buf);
			}
			else
			{
				// File no have content
				kernel->machine->WriteRegister(2, -2);
			}
			delete buf;
			IncreasePC();
			return;
		}
		case SC_Write:
		{
			int virtAddr = kernel->machine->ReadRegister(4);
			int charcount = kernel->machine->ReadRegister(5);
			int FileId = kernel->machine->ReadRegister(6); 
			int startPos;
			int endPos;
			char *buf;
			// file descriptor table only 20 slot
			if (FileId < 0 || FileId > 19)
			{
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				return;
			}
			// Check file is exist 
			if (kernel->fileSystem->openf[FileId] == NULL)
			{
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				return;
			}
			// Read-only and stdin can not write
			if (kernel->fileSystem->openf[FileId]->type == 1 || kernel->fileSystem->openf[FileId]->type == 2)
			{
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				return;
			}
			startPos = kernel->fileSystem->openf[FileId]->GetCurrentPos(); // the start position of file cursor when read
			buf = User2System(virtAddr, charcount); 
			if (kernel->fileSystem->openf[FileId]->type == 0)
			{
				if ((kernel->fileSystem->openf[FileId]->Write(buf, charcount)) > 0)
				{
					// the final position of file cursor when read
					endPos = kernel->fileSystem->openf[FileId]->GetCurrentPos();
					// returns the number of bytes written
					kernel->machine->WriteRegister(2, endPos - startPos);
					delete buf;
					IncreasePC();
					return;
				}
			}
			// stdout
			if (kernel->fileSystem->openf[FileId]->type == 3) 
			{
				buf = User2System(virtAddr,255);
				int length = 0;
				while (buf[length] != 0) length++;
				// write the string to console
				kernel->synchConsoleOut->Write(buf, length); 
				// returns the number of bytes written
				kernel->machine->WriteRegister(2, length);
				delete buf;
				IncreasePC();
				return;
			}
		}
		case SC_Seek:
		{
			int pos = kernel->machine->ReadRegister(4);
			int id = kernel->machine->ReadRegister(5);

			if (id >= 20 || id < 0){
				DEBUG(dbgSys, "Exceed number of file that can be opened\n");
				kernel->machine->WriteRegister(2, -1);

				return;
			}
			
			if (id == 0 || id == 1){
				DEBUG(dbgSys, "Cannot seek on console\n");
				kernel->machine->WriteRegister(2, -1);
				return;
			}

			OpenFile *openfile = kernel->fileSystem->openf[id];
			if (openfile == NULL){
				DEBUG(dbgSys, "File is not opened\n");
				kernel->machine->WriteRegister(2, -1);
				return;
			}

			int fileLength = openfile->Length();
			pos = (pos == -1)? fileLength : pos;

			if (fileLength < pos || pos < 0){
				DEBUG(dbgSys, "Position exceeds file\n");
				kernel->machine->WriteRegister(2, -1);

				delete openfile;
				return;
			}

			OpenFile* f = kernel->fileSystem->openf[id];
			f->Seek(pos);
			kernel->machine->WriteRegister(2, f->GetCurrentOffSet());
			IncreasePC();
			return;
		}

		case SC_Remove:
		{
			int virtAddr;
			char* filename = NULL;
			virtAddr = kernel->machine->ReadRegister(4);
			filename = User2System(virtAddr, MaxFileLength + 1);
			
			if (kernel->fileSystem->Remove(filename) == 0){
				kernel->machine->WriteRegister(2, -1);
				delete[] filename;
				return;
			}
			kernel->machine->WriteRegister(2, 0);
			IncreasePC();
			delete[] filename;
			return;
		}
		case SC_SocketTCP: 
		{
			int socket_fd;
			if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			{
				DEBUG(dbgSys, "Create Socket failed\n");
				kernel->machine->WriteRegister(2, -1);
			}
			else{
				DEBUG(dbgSys, "Create Socket success\n");
				kernel->machine->WriteRegister(2, socket_fd);
			}
			IncreasePC();
			return;

			/*
			int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

			if (socket_fd < 0)
			{
				DEBUG(dbgSys, "Create Socket failed\n");
				kernel->machine->WriteRegister(2, -1);

				IncreasePC();
				return;
			}

			int freeslot;
			if ((freeslot = kernel->fileSystem->FindFreeSlotInSocket()) == -1){
				DEBUG(dbgSys, "Run out of slot");
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				return;
			}

			OpenFile* socket_file = new OpenFile(socket_fd); // Tạo 1 object OpenFile để mình quản lý, socket_fd là file descriptor mà hệ điều hành mở file và trả về cho mình

			kernel->fileSystem->open_socket[freeslot] = socket_file;
			
			DEBUG(dbgSys, "Create Socket success\n");
			DEBUG(dbgSys, freeslot);
			kernel->machine->WriteRegister(2, socket_fd);	 // Chỗ này chị phải trả về socket_fd mà system cho mình
															// Chị thử trả về freeslot thì hong connect đc em
			
			IncreasePC();
			return;
			*/
		}
		
		case SC_Connect:
		{
			/*
			Hàm để connect đến server
			Input: socketid của client
				   ip của server
				   port của server
			Output: 0 nếu thành công, -1 nếu lỗi
			*/	
			DEBUG(dbgSys, "Start connecting\n");
			int socketid = kernel->machine->ReadRegister(4);
			int virtAddr = kernel->machine->ReadRegister(5);
			int port = kernel->machine->ReadRegister(6);

			char* ip = User2System(virtAddr, MaxFileLength + 1); // Copy ip của server vào vùng nhớ của system
			DEBUG(dbgSys, ip);
			struct sockaddr_in serv_addr;

			// Gán địa chỉ ip và port của server vào serv_addr
			// serv_addr là transport address của server, client sẽ dùng để kết nối với server
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_addr.s_addr = inet_addr(ip);
			serv_addr.sin_port = htons(port);

			//Convert IPv4 and IPv6 addresses from text to binary form (Geeksforgeeks)
			if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0){
				kernel->machine->WriteRegister(2, -1);

				IncreasePC();
				return;
			}

			// Connect vào server với socketid của client và địa chỉ của server serv_addr
			if (connect(socketid, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
				DEBUG(dbgSys, "Connection failed\n");
				kernel->machine->WriteRegister(2, -1);

				IncreasePC();
				return;
			}
			DEBUG(dbgSys, "\nConnection success\n");
			kernel->machine->WriteRegister(2, 0);
			delete[] ip;
			IncreasePC();
			return;
		}
		
		case SC_Send:
		{
			/*
			Hàm để gửi chuỗi cho server
			Input: socketid: socketid của client
				   buffer: chuỗi mà client muốn gửi
				   len: chiều dài của chuỗi
			Output: Số lượng byte gửi đi
			*/
			int socketid = kernel->machine->ReadRegister(4);
			int virtAddr = kernel->machine->ReadRegister(5);
			int len = kernel->machine->ReadRegister(6);

			char* buffer = NULL;
			buffer = User2System(virtAddr, len + 1); // Copy chuỗi muốn gửi vào vùng nhớ của system

			if (buffer == NULL){
				DEBUG(dbgSys, "Not enough memory in system\n");
				kernel->machine->WriteRegister(2, -1);
				return;
			}

			DEBUG(dbgSys, "is sending");
			send(socketid, buffer, len, 0);  // Gọi hàm gửi
			DEBUG(dbgSys, "Finsish sending");

			kernel->machine->WriteRegister(2, sizeof(buffer)/sizeof(buffer[0]));
			
			delete[] buffer;
			IncreasePC();
			return;
		}
		
		case SC_Receive:
		{
			/*
			Hàm để nhận chuỗi cho server gửi
			Input: socketid: socketid (là file descriptor cho socket file mà khi mình tạo socket system cho mình) của client
				   buffer: 1 chuỗi (chị nghĩ là rỗng) dùng để nhận chuỗi của server gửi
				   len: chiều dài tối đa của chuỗi mình có thể nhận (vd: chuỗi: "hello", len = 1024 vẫn đc vì nó là max length)
			Output: số byte nhận đc
			*/
			DEBUG(dbgSys, "Is receiving");
			int socketid = kernel->machine->ReadRegister(4);
			int virtAddr = kernel->machine->ReadRegister(5);
			int len = kernel->machine->ReadRegister(6);

			char* buffer = new char[1024];  // Tạo 1 chuỗi trong system
			if (buffer == NULL){
				DEBUG(dbgSys, "Not enough memory in system\n");
				kernel->machine->WriteRegister(2, -1);

				IncreasePC();
				return;
			}

			int valread = read(socketid, buffer, len);  // Đọc chuỗi nhận đc vào buffer (là chuỗi trong bộ nhớ của system)
			System2User(virtAddr, len, buffer);         // Copy chuỗi trong bộ nhớ của system vào bộ nhớ của người dùng
			kernel->machine->WriteRegister(2, sizeof(buffer)/sizeof(buffer[0]));
			DEBUG(dbgSys, "Receiving string: ");
			DEBUG(dbgSys, buffer);
			delete[] buffer;
			IncreasePC();
			return;
		}

		case SC_CloseSocket:
		{
			int socketid;
			socketid = kernel->machine->ReadRegister(4);

			if (close(socketid) == -1){
				kernel->machine->WriteRegister(2, -1);
				DEBUG(dbgSys, "Close socket failed\n");
				IncreasePC();
				return;
			}

			DEBUG(dbgSys, "Close socket successful\n");
			IncreasePC();
			return;
		}

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
			IncreasePC();
			return;
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
