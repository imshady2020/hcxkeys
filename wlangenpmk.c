#define _GNU_SOURCE
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdio_ext.h>
#include <openssl/evp.h>
#include "common.h"
#include "common.c"

#define COWPATTY_SIGNATURE 0x43575041L

struct cow_head
{
 uint32_t magic;
 uint8_t reserved1[3];
 uint8_t essidlen;
 uint8_t essid[32];
};
typedef struct cow_head cow_head_t;
#define	COWHEAD_SIZE (sizeof(cow_head_t))

/*===========================================================================*/
/* globale Variablen */

uint8_t progende = FALSE;
/*===========================================================================*/
void programmende(int signum)
{
if((signum == SIGINT) || (signum == SIGTERM) || (signum == SIGKILL))
	{
	progende = TRUE;
	}
return;
}
/*===========================================================================*/
size_t chop(char *buffer, size_t len)
{
char *ptr = buffer +len -1;

while(len)
	{
	if (*ptr != '\n')
		break;
	*ptr-- = 0;
	len--;
	}

while(len)
	{
	if (*ptr != '\r')
		break;
	*ptr-- = 0;
	len--;
	}
return len;
}
/*---------------------------------------------------------------------------*/
int fgetline(FILE *inputstream, size_t size, char *buffer)
{
if(feof(inputstream))
	return -1;
char *buffptr = fgets (buffer, size, inputstream);

if(buffptr == NULL)
	return -1;

size_t len = strlen(buffptr);
len = chop(buffptr, len);
return len;
}
/*===========================================================================*/
void filecombiout(FILE *fhcombi, FILE *fhascii, FILE *fhasciipw, FILE *fhcow)
{
int c;
int combilen;
int essidlen;
int pwlen;
int p;
int cr;
cow_head_t cow;
uint8_t cowreclen = 0;
long int pmkcount = 0;
long int skippedcount = 0;

char combiline[256];

uint8_t essidbuf[128];
uint8_t pwbuf[128];

unsigned char pmk[64];


signal(SIGINT, programmende);

if(fhcow != NULL)
	{
	memset(&cow, 0, COWHEAD_SIZE);
	cow.magic = COWPATTY_SIGNATURE;
	memset(&cow.essid, 0, 32);
	cow.essidlen = 0;
	cow.reserved1[2] = 1;
	cr = fwrite(&cow, COWHEAD_SIZE, 1, fhcow);
	if(cr != 1)
		{
		fprintf(stderr, "error writing cowpatty file\n");
		exit(EXIT_FAILURE);
		}
	}

while((progende != TRUE) && ((combilen = fgetline(fhcombi, 256, combiline)) != -1))
	{
	if(combilen < 10)
		{
		skippedcount++;
		continue;
		}

	p = getdelimiterpos((uint8_t*)combiline, ':');
	essidlen = p;
	pwlen = combilen -p -1;

	memset(&essidbuf, 0, 128);
	if(is_hexify((uint8_t*)combiline, essidlen) == true)
		{
		essidlen = do_unhexify((uint8_t*)combiline, essidlen, essidbuf, 128);
		}
	else
		memcpy(&essidbuf, &combiline, essidlen);

	if((essidlen < 1) || (essidlen > 32))
		{
		skippedcount++;
		continue;
		}

	if(is_hexify((uint8_t*)combiline +p +1, pwlen) == true)
		{
		pwlen = do_unhexify((uint8_t*)combiline +p +1, pwlen, pwbuf, 128);
		}
	else
		{
		memcpy(pwbuf, &combiline[p +1], pwlen);
		if((pwlen < 8) || (pwlen > 63))
			{
			printf("\x1B[31m%s\x1B[0m\n", combiline);
			skippedcount++;
			continue;
			}
		}

	if((pwlen < 8) || (pwlen > 64))
		{
		skippedcount++;
		continue;
		}

	if( PKCS5_PBKDF2_HMAC_SHA1((const char*)pwbuf, pwlen, essidbuf, essidlen, 4096, 32, pmk) != 0 )
		{
		if(fhcow != NULL)
			{
			cowreclen = sizeof(cowreclen) + pwlen + 32;
			cr = fwrite(&cowreclen, sizeof(cowreclen), 1, fhcow);
			if(cr != 1)
				{
				fprintf(stderr, "error writing cowpatty file\n");
				exit(EXIT_FAILURE);
				}
			cr = fwrite(&pwbuf, sizeof(uint8_t), pwlen, fhcow);
			cr = fwrite(&pmk, sizeof(uint8_t), 32, fhcow);
			if(cr != 32)
				{
				fprintf(stderr, "error writing cowpatty file\n");
				exit(EXIT_FAILURE);
				}
			}
		for(c = 0; c< 32; c++)
			{
			if(fhascii != NULL)
				fprintf(fhascii, "%02x", pmk[c]);

			if(fhasciipw != NULL)
				fprintf(fhasciipw, "%02x", pmk[c]);
			}
		if(fhascii != NULL)
			fprintf(fhascii, "\n");

		if(fhasciipw != NULL)
			fprintf(fhasciipw, ":%s\n", pwbuf);



		pmkcount++;
		if((pmkcount %1000) == 0)
			printf("\r%ld", pmkcount);
		}
	}

printf("\r%ld plainmasterkeys generated, %ld password(s) skipped\n", pmkcount, skippedcount);
return;
}
/*===========================================================================*/
void filepmkout(FILE *pwlist, FILE *fhascii,  FILE *fhasciipw, FILE *fhcow, char *essidname, uint8_t essidlen)
{
int pwlen;
int c;
int cr;
cow_head_t cow;
uint8_t cowreclen = 0;
long int pmkcount = 0;
long int skippedcount = 0;
unsigned char essid[32];
char password[64];
unsigned char pmk[64];


signal(SIGINT, programmende);
memcpy(&essid, essidname, essidlen);
if((fhcow != NULL) && (essidname != NULL))
	{
	memset(&cow, 0, COWHEAD_SIZE);
	cow.magic = COWPATTY_SIGNATURE;
	memcpy(cow.essid, essidname, essidlen);
	cow.essidlen = essidlen;
	cr = fwrite(&cow, COWHEAD_SIZE, 1, fhcow);
	if(cr != 1)
		{
		fprintf(stderr, "error writing cowpatty file\n");
		exit(EXIT_FAILURE);
		}
	}

while((progende != TRUE) && ((pwlen = fgetline(pwlist, 64, password)) != -1))
	{
	if((pwlen < 8) || pwlen > 63)
		{
		skippedcount++;
		continue;
		}

	if(PKCS5_PBKDF2_HMAC(password, pwlen, essid, essidlen, 4096, EVP_sha1(), 32, pmk) != 0)
		{
		if(fhcow != NULL)
			{
			cowreclen = sizeof(cowreclen) + pwlen + 32;
			cr = fwrite(&cowreclen, sizeof(cowreclen), 1, fhcow);
			if(cr != 1)
				{
				fprintf(stderr, "error writing cowpatty file\n");
				exit(EXIT_FAILURE);
				}
			fprintf(fhcow, "%s", password);
			cr = fwrite(&pmk, sizeof(uint8_t), 32, fhcow);
			if(cr != 32)
				{
				fprintf(stderr, "error writing cowpatty file\n");
				exit(EXIT_FAILURE);
				}
			}
		for(c = 0; c< 32; c++)
			{
			if(fhascii != NULL)
				fprintf(fhascii, "%02x", pmk[c]);

			if(fhasciipw != NULL)
				fprintf(fhasciipw, "%02x", pmk[c]);
			}
		if(fhascii != NULL)
			fprintf(fhascii, "\n");

		if(fhasciipw != NULL)
			fprintf(fhasciipw, ":%s\n", password);

		pmkcount++;
		if((pmkcount %1000) == 0)
			printf("\r%ld", pmkcount);
		}
	}	

printf("\r%ld plainmasterkeys generated, %ld password(s) skipped\n", pmkcount, skippedcount);
return;
}
/*===========================================================================*/
void singlepmkout(char *pwname, int pwlen, char *essidname, int essidlen)
{
int c;

unsigned char essid[32];
unsigned char pmk1[64];
unsigned char pmk256[64];

memset(&essid, 0, 32);
memcpy(&essid, essidname, essidlen);

fprintf(stdout, "\n"
		"essid (networkname)....: %s\n"
		"password...............: %s\n"
		, essidname, pwname);


if(PKCS5_PBKDF2_HMAC(pwname, pwlen, essid, essidlen, 4096, EVP_sha1(), 32, pmk1) != 0)
	{
	printf("plainmasterkey (SHA1)..: ");
	for(c = 0; c< 32; c++)
		{
		printf("%02x", pmk1[c]);
		}
	printf("\n");
	}
if(PKCS5_PBKDF2_HMAC(pwname, pwlen, essid, essidlen, 4096, EVP_sha256(), 32, pmk256) != 0)
	{
	printf("plainmasterkey (SHA256): ");
	for(c = 0; c< 32; c++)
		{
		printf("%02x", pmk256[c]);
		}
	printf("\n\n");
	}

return;	
}
/*===========================================================================*/
static void usage(char *eigenname)
{
printf("%s %s (C) %s ZeroBeat\n"
	"usage: %s <options>\n"
	"\n"
	"options:\n"
	"-e <essid>    : input single essid (networkname: 1 .. 32 characters) requires -p\n"
	"-p <password> : input single password (8 .. 63 characters) requires -e\n"
	"-i <file>     : input passwordlist\n"
	"-I <file>     : input combilist (essid:password)\n"
	"-a <file>     : output plainmasterkeys as ASCII file (hashcat -m 2501)\n"
	"-A <file>     : output plainmasterkeys:password as ASCII file\n"
	"-c <file>     : output cowpatty hashfile (existing file will be replaced)\n"
	"-h            : this help\n"
	"\n", eigenname, VERSION, VERSION_JAHR, eigenname);
exit(EXIT_FAILURE);
}
/*===========================================================================*/
int main(int argc, char *argv[])
{
FILE *fhpwlist = NULL;
FILE *fhascii = NULL;
FILE *fhasciipw = NULL;
FILE *fhcow = NULL;
FILE *fhcombi = NULL;
int auswahl;

int pwlen = 0;
uint8_t essidlen = 0;

char *eigenname = NULL;
char *eigenpfadname = NULL;
char *pwname = NULL;
char *essidname = NULL;


eigenpfadname = strdupa(argv[0]);
eigenname = basename(eigenpfadname);

setbuf(stdout, NULL);
while ((auswahl = getopt(argc, argv, "p:e:i:I:a:A:c:h")) != -1)
	{
	switch (auswahl)
		{
		case 'e':
		essidname = optarg;
		essidlen = strlen(essidname);
		if((essidlen < 1) || (essidlen > 32))
			{
			fprintf(stderr, "error wrong essid len\n");
			exit(EXIT_FAILURE);
			}
		break;

		case 'p':
		pwname = optarg;
		pwlen = strlen(pwname);
		if((pwlen < 8) || (pwlen > 63))
			{
			fprintf(stderr, "error wrong password len\n");
			exit(EXIT_FAILURE);
			}
		break;

		case 'i':
		if((fhpwlist = fopen(optarg, "r")) == NULL)
			{
			fprintf(stderr, "error opening %s\n", optarg);
			exit(EXIT_FAILURE);
			}
		break;

		case 'I':
		if((fhcombi = fopen(optarg, "r")) == NULL)
			{
			fprintf(stderr, "error opening %s\n", optarg);
			exit(EXIT_FAILURE);
			}
		break;

		case 'a':
		if((fhascii = fopen(optarg, "a")) == NULL)
			{
			fprintf(stderr, "error opening %s\n", optarg);
			exit(EXIT_FAILURE);
			}
		break;

		case 'A':
		if((fhasciipw = fopen(optarg, "a")) == NULL)
			{
			fprintf(stderr, "error opening %s\n", optarg);
			exit(EXIT_FAILURE);
			}
		break;

		case 'c':
		if((fhcow = fopen(optarg, "w")) == NULL)
			{
			fprintf(stderr, "error opening %s\n", optarg);
			exit(EXIT_FAILURE);
			}
		break;

		case 'h':
		usage(eigenname);
		break;

		default:
		usage(eigenname);
		break;
		}
	}

if((essidname != NULL) && (pwname != NULL))
	singlepmkout(pwname, pwlen, essidname, essidlen);

else if((essidname != NULL) && (fhpwlist != NULL))
	filepmkout(fhpwlist, fhascii, fhasciipw, fhcow, essidname, essidlen);

else if(fhcombi != NULL)
	filecombiout(fhcombi, fhascii, fhasciipw, fhcow);

if(fhcombi != NULL)
	fclose(fhcombi);

if(fhpwlist != NULL)
	fclose(fhpwlist);

if(fhascii != NULL)
	fclose(fhascii);

return EXIT_SUCCESS;
}
