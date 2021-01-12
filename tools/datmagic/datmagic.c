/*

	Beta version 2  -  Jan. 11th 2021
	by: mahoneyt944 - MAME 2003-Plus Team.

	notes: - currently writes to a log, will work on writing to a html table next.

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXCHAR 200

void banner(void)
{
	printf("\n'########:::::'###::::'########:\n");
	printf(  " ##.... ##:::'## ##:::... ##..::\n");
	printf(  " ##:::: ##::'##:. ##::::: ##::::\n");
	printf(  " ##:::: ##:'##:::. ##:::: ##::::\n");
	printf(  " ##:::: ##: #########:::: ##::::\n");
	printf(  " ##:::: ##: ##.... ##:::: ##::::\n");
	printf(  " ########:: ##:::: ##:::: ##::::\n");
	printf(  "........:::..:::::..:::::..:::::\n");
	printf(  "'##::::'##::::'###:::::'######:::'####::'######::\n");
	printf(  " ###::'###:::'## ##:::'##... ##::. ##::'##... ##:\n");
	printf(  " ####'####::'##:. ##:: ##:::..:::: ##:: ##:::..::\n");
	printf(  " ## ### ##:'##:::. ##: ##::'####:: ##:: ##:::::::\n");
	printf(  " ##. #: ##: #########: ##::: ##::: ##:: ##:::::::\n");
	printf(  " ##:.:: ##: ##.... ##: ##::: ##::: ##:: ##::: ##:\n");
	printf(  " ##:::: ##: ##:::: ##:. ######:::'####:. ######::\n");
	printf(  "..:::::..::..:::::..:::......::::....:::......:::\n");
}

int main()
{
	banner();
	char* dat = "mame2003-plus.xml";
	FILE *read;
	FILE *write;
	char readline[MAXCHAR];

	/***************** ID tags *****************/
	char game_id[]     = "<game name=\"";
	char sample_id[]   = "<sample name=\"";
	char bios_id[]     = "<biosset name=\"";
	char driver_id[]   = "<driver status=\"";
	char endgame_id[]  = "</game>";

	/***************** Search fields *****************/
	char sampleof[]    = "sampleof=\"";
	char color[]       = "color=\"";
	char sound[]       = "sound=\"";

	/***************** Flags and counters *****************/
	int found, parentsample, clonesample, realgame, bios = 0;

	/***************** Allocate memory to use *****************/
	char *romname      = malloc(sizeof(char) * 20);
	char *driverstatus = malloc(sizeof(char) * 20);
	char *colorstatus  = malloc(sizeof(char) * 20);
	char *soundstatus  = malloc(sizeof(char) * 20);
	char *sampleused   = malloc(sizeof(char) * 20);
	char *biosused     = malloc(sizeof(char) * 2);


	/***************** Try to open the DAT file *****************/
	read = fopen(dat, "r");
	printf("\nTrying to open the DAT file:  %s\n", dat);

	if (read == NULL)
	{
		printf("Could not open the DAT file:  Not found in directory.\n\n");
		return 1;
	}

	/***************** Open log to write to *****************/
	write = fopen("log.txt", "w");
	printf("\nProcessing DAT now.\n");


	/***************** Search the DAT file line by line and process IDs *****************/
	while ( fgets(readline, MAXCHAR, read) != NULL )
	{
		char *target = NULL;
		char *start, *end;

		/***************** Read game tag *****************/
		if ( start = strstr( readline, game_id ) )
		{
			realgame = 1;
			start += strlen( game_id );
			if ( end = strstr( start, "\"" ) )
			{
				target = ( char * )malloc( end - start + 1 );
				memcpy( target, start, end - start );
				target[end - start] = '\0';

				strcpy( romname, target );
			}

			/***************** Check for sampleof *****************/
			if ( start = strstr( readline, sampleof ) )
			{
				start += strlen( sampleof );
				if ( end = strstr( start, "\"" ) )
				{
					target = ( char * )malloc( end - start + 1 );
					memcpy( target, start, end - start );
					target[end - start] = '\0';
					clonesample = 1;

					strcpy( sampleused, target );
				}
			}

			found++;
		}

		/***************** Read sample tag *****************/
		else if ( (start = strstr( readline, sample_id )) && !(parentsample) )
		{
			parentsample = 1;
		}

		/***************** Read bios tag *****************/
		else if ( (start = strstr( readline, bios_id )) && !(bios) )
		{
			bios = 1;
		}

		/***************** Read driver status tag *****************/
		else if ( start = strstr( readline, driver_id ) )
		{
			start += strlen( driver_id );
			if ( end = strstr( start, "\"" ) )
			{
				target = ( char * )malloc( end - start + 1 );
				memcpy( target, start, end - start );
				target[end - start] = '\0';

				strcpy( driverstatus, target );
			}

			/***************** Check for color *****************/
			if ( start = strstr( readline, color ) )
			{
				start += strlen( color );
				if ( end = strstr( start, "\"" ) )
				{
					target = ( char * )malloc( end - start + 1 );
					memcpy( target, start, end - start );
					target[end - start] = '\0';

					strcpy( colorstatus, target );
				}
			}

			/***************** Check for sound *****************/
			if ( start = strstr( readline, sound ) )
			{
				start += strlen( sound );
				if ( end = strstr( start, "\"" ) )
				{
					target = ( char * )malloc( end - start + 1 );
					memcpy( target, start, end - start );
					target[end - start] = '\0';

					strcpy( soundstatus, target );
				}
			}
		}

		/***************** Read end game tag *****************/
		else if ( start = strstr( readline, endgame_id ) )
		{
			/***************** Configure parent sample *****************/
			if ( parentsample && !clonesample ) strcpy( sampleused, romname );
			else if ( !parentsample && !clonesample ) strcpy( sampleused, "0" );

			/***************** Configure bios *****************/
			if ( bios ) strcpy( biosused, "1\0" );
			else strcpy( biosused, "0\0" );

			/***************** Write data out *****************/
			if ( realgame )
			{
				fputs( romname, write );
				fputs( ":", write );
				fputs( driverstatus, write );
				fputs( ":", write );
				fputs( colorstatus, write );
				fputs( ":", write );
				fputs( soundstatus, write );
				fputs( ":", write );
				fputs( sampleused, write );
				fputs( ":", write );
				fputs( biosused, write );
				fputs( "\n", write );
			}


			/***************** Reset flags *****************/
			parentsample = 0;
			clonesample = 0;
			realgame  = 0;
			bios = 0;
		}

		free( target );

	}

	/***************** Free memory *****************/
	free( romname );
	free( driverstatus );
	free( colorstatus );
	free( soundstatus );
	free( sampleused );
	free( biosused );

	/***************** Close up our files *****************/
	printf("Closing DAT file.\n");
	fclose(read);
	fclose(write);

	/***************** Total games found *****************/
	printf("\nRoms found and processed:  %i\n\n", found);

	return 0;
}
