#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <PCSC/wintypes.h>
#include <PCSC/winscard.h>
//#include "/usr/local/include/PCSC/wintypes.h"
//#include "/usr/local/include/PCSC/winscard.h"

#define PCSCD_PATH "/home/seki/School/18p/PA193/PCSC/PCSC-installed/sbin/pcscd"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* PCSC error message pretty print */
#define PCSC_ERROR(rv, text) \
if (rv != SCARD_S_SUCCESS) \
{ \
	printf(text ": %s (0x%lX)\n", pcsc_stringify_error(rv), rv); \
	goto end; \
} \
else \
{ \
	printf(text ": OK\n\n"); \
}



int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: ./tester <input>\n");
    return 0;
  }
  
  pid_t pid = fork();
  if (pid == 0) {
    pid_t pid2 = fork();

    if (pid2 == 0){
      if (execl("/usr/local/bin/vicc", NULL) == -1){
        perror("execl");
      }
      return 0;
    }
    if (pid2 == -1){
      perror("fork");
      return -1;
    }

    FILE* nStderr = fopen("pcscd-stderr.txt", "w");
    if (nStderr == NULL) {
       perror("cannot redirect stderr from pcscd");
       return 1;
    }
    dup2(fileno(nStderr), STDERR_FILENO);
    fclose(nStderr);
    if (execl(PCSCD_PATH, "-d", "-f", NULL) == -1){
      perror("execl");
    }
    return 0;
  }
  if (pid == -1){
    perror("fork");
    return -1;
  }

  sleep(1);

  //SAMPLE
 
  FILE* fh;
  fh = fopen(argv[1], "rb");
  if (fh == NULL) {
    perror("cannot open file");
    goto end;
  }

  //reader name
  //share type
  //data
  //pbreader
  char INReaderName[1000];
  fgets(INReaderName, 999, fh);
  unsigned long long rnl = strlen(INReaderName);
  if (rnl >= 1 && INReaderName[rnl-1] == '\n') INReaderName[rnl-1] = '\0';
  unsigned long INSharedType = fgetc(fh);
  
  DWORD dwSendLength, dwRecvLength;
  BYTE pbSendBuffer[1000];// = { 0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
  dwSendLength = fread(pbSendBuffer, 1, 1000, fh);
  fclose(fh);
 
  printf("Reader|%s|\n", INReaderName);
 
  LONG rv;
	SCARDCONTEXT hContext;
	DWORD dwReaders;
	LPSTR mszReaders = NULL;
	char *ptr, **readers = NULL;
	int nbReaders;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol, dwReaderLen, dwState, dwProt, dwAtrLen;
	BYTE pbAtr[MAX_ATR_SIZE] = "";
	char pbReader[MAX_READERNAME] = "";
	int reader_nb;
	unsigned int i;
	const SCARD_IO_REQUEST *pioSendPci;
	SCARD_IO_REQUEST pioRecvPci;
	BYTE pbRecvBuffer[10];
	//BYTE pbSendBuffer[] = { 0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
	//DWORD dwSendLength, dwRecvLength;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext: Cannot Connect to Resource Manager %lX\n", rv);
		return EXIT_FAILURE;
	}

	/* Retrieve the available readers list. */
	dwReaders = SCARD_AUTOALLOCATE;
	rv = SCardListReaders(hContext, NULL, (LPSTR)&mszReaders, &dwReaders);
	PCSC_ERROR(rv, "SCardListReaders")

	/* Extract readers from the null separated string and get the total number of readers */
	nbReaders = 0;
	ptr = mszReaders;
	while (*ptr != '\0')
	{
		ptr += strlen(ptr)+1;
		nbReaders++;
	}

	if (nbReaders == 0)
	{
		printf("No reader found\n");
		goto end;
	}

	/* allocate the readers table */
	readers = calloc(nbReaders, sizeof(char *));
	if (NULL == readers)
	{
		printf("Not enough memory for readers[]\n");
		goto end;
	}

	/* fill the readers table */
	nbReaders = 0;
	ptr = mszReaders;
	while (*ptr != '\0')
	{
		printf("%d: %s\n", nbReaders, ptr);
		readers[nbReaders] = ptr;
		ptr += strlen(ptr)+1;
		nbReaders++;
	}

	if (argc > 1)
	{
		reader_nb = atoi(argv[1]);
		if (reader_nb < 0 || reader_nb >= nbReaders)
		{
			printf("Wrong reader index: %d\n", reader_nb);
			goto end;
		}
	}
	else
		reader_nb = 0;

	/* connect to a card */
	dwActiveProtocol = -1;
	rv = SCardConnect(hContext, /*readers[reader_nb]*/ INReaderName, INSharedType,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
	printf(" Protocol: %ld\n", dwActiveProtocol);
	PCSC_ERROR(rv, "SCardConnect")

	/* get card status */
	dwAtrLen = sizeof(pbAtr);
	dwReaderLen = sizeof(pbReader);
	rv = SCardStatus(hCard, /*NULL*/ pbReader, &dwReaderLen, &dwState, &dwProt,
		pbAtr, &dwAtrLen);
	printf(" Reader: %s (length %ld bytes)\n", pbReader, dwReaderLen);
	printf(" State: 0x%lX\n", dwState);
	printf(" Prot: %ld\n", dwProt);
	printf(" ATR (length %ld bytes):", dwAtrLen);
	for (i=0; i<dwAtrLen; i++)
		printf(" %02X", pbAtr[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardStatus")

    switch(dwActiveProtocol)
    {
        case SCARD_PROTOCOL_T0:
            pioSendPci = SCARD_PCI_T0;
            break;
        case SCARD_PROTOCOL_T1:
            pioSendPci = SCARD_PCI_T1;
            break;
        default:
            printf("Unknown protocol\n");
            goto end;
    }

	/* exchange APDU */
	//dwSendLength = sizeof(pbSendBuffer);
	dwRecvLength = sizeof(pbRecvBuffer);
	printf("Sending: ");
	for (i=0; i<dwSendLength; i++)
		printf("%02X ", pbSendBuffer[i]);
	printf("\n");
	rv = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
		&pioRecvPci, pbRecvBuffer, &dwRecvLength);
	printf("Received: ");
	for (i=0; i<dwRecvLength; i++)
		printf("%02X ", pbRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardTransmit")

	/* card disconnect */
	rv = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
	PCSC_ERROR(rv, "SCardDisconnect")

	/* connect to a card */
	dwActiveProtocol = -1;
	rv = SCardConnect(hContext, /*readers[reader_nb]*/ INReaderName, INSharedType,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
	printf(" Protocol: %ld\n", dwActiveProtocol);
	PCSC_ERROR(rv, "SCardConnect")

	/* exchange APDU */
	//dwSendLength = sizeof(pbSendBuffer);
	dwRecvLength = sizeof(pbRecvBuffer);
	printf("Sending: ");
	for (i=0; i<dwSendLength; i++)
		printf("%02X ", pbSendBuffer[i]);
	printf("\n");
	rv = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
		&pioRecvPci, pbRecvBuffer, &dwRecvLength);
	printf("Received: ");
	for (i=0; i<dwRecvLength; i++)
		printf("%02X ", pbRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardTransmit")

	/* card reconnect */
	rv = SCardReconnect(hCard, INSharedType,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, SCARD_LEAVE_CARD,
		&dwActiveProtocol);
	PCSC_ERROR(rv, "SCardReconnect")

	/* get card status */
	dwAtrLen = sizeof(pbAtr);
	dwReaderLen = sizeof(pbReader);
	rv = SCardStatus(hCard, /*NULL*/ pbReader, &dwReaderLen, &dwState, &dwProt,
		pbAtr, &dwAtrLen);
	printf(" Reader: %s (length %ld bytes)\n", pbReader, dwReaderLen);
	printf(" State: 0x%lX\n", dwState);
	printf(" Prot: %ld\n", dwProt);
	printf(" ATR (length %ld bytes):", dwAtrLen);
	for (i=0; i<dwAtrLen; i++)
		printf(" %02X", pbAtr[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardStatus")

	/* get card status change */
	{
		/* check only one reader */
		SCARD_READERSTATE rgReaderStates[1];

		rgReaderStates[0].szReader = pbReader;
		rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;

		rv = SCardGetStatusChange(hContext, 0, rgReaderStates, 1);
		printf(" state: 0x%04lX\n", rgReaderStates[0].dwEventState);
		PCSC_ERROR(rv, "SCardGetStatusChange")
	}

	/* begin transaction */
	rv = SCardBeginTransaction(hCard);
	PCSC_ERROR(rv, "SCardBeginTransaction")

	/* exchange APDU */
	//dwSendLength = sizeof(pbSendBuffer);
	dwRecvLength = sizeof(pbRecvBuffer);
	printf("Sending: ");
	for (i=0; i<dwSendLength; i++)
		printf("%02X ", pbSendBuffer[i]);
	printf("\n");
	rv = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
		&pioRecvPci, pbRecvBuffer, &dwRecvLength);
	printf("Received: ");
	for (i=0; i<dwRecvLength; i++)
		printf("%02X ", pbRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardTransmit")

	/* end transaction */
	rv = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
	PCSC_ERROR(rv, "SCardEndTransaction")

	/* card disconnect */
	rv = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
	PCSC_ERROR(rv, "SCardDisconnect")

end:
	/* free allocated memory */
	if (mszReaders)
		SCardFreeMemory(hContext, mszReaders);

	/* We try to leave things as clean as possible */
	rv = SCardReleaseContext(hContext);
	if (rv != SCARD_S_SUCCESS)
		printf("SCardReleaseContext: %s (0x%lX)\n", pcsc_stringify_error(rv),
			rv);

	if (readers)
		free(readers);

//	return EXIT_SUCCESS;


  //SAMPLE END
 
  int status;
  pid_t result = waitpid(pid, &status, WNOHANG);
  if (result == 0) {
    kill(pid, SIGKILL);
  } else if (result == -1) {
    perror("waitpid");
    kill(pid, SIGKILL);
    return -1;
  } else {
    if (WIFEXITED(status)) {
      //printf("exited, status=%d\n", WEXITSTATUS(status));
      return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      //printf("killed by signal %d\n", WTERMSIG(status));
      kill(getpid(), WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      //printf("stopped by signal %d\n", WSTOPSIG(status));
      kill(getpid(), WSTOPSIG(status));
    } else if (WIFCONTINUED(status)) {
      //printf("continued\n");
      kill(pid, SIGKILL);
    }
  }
  return 0;
}
