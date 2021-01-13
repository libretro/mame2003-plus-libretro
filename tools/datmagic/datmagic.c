/*

	Beta version 3  -  Jan. 12th 2021
	by: mahoneyt944 - MAME 2003-Plus Team.

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
	int found=0, parentsample=0, clonesample=0, realgame=0, bios=0;

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

	/***************** Open new html file to write to *****************/
	write = fopen("datmagic.html", "w");
	printf("\nProcessing DAT now.\n");


	/***************** Write out html table header *****************/
	fputs( "<style type=\"text/css\">\n", write );
	fputs( "\ttable, th, td {border: 1px solid black; border-collapse: collapse;}\n", write );
	fputs( "\tth, td {padding: 5px;}\n", write );
	fputs( "\tth {text-align: left;}\n", write );
	fputs( "</style>\n\n", write );
	fputs( "<h2>MAME 2003-Plus</h2>\n\n", write );
	fputs( "<table style=\"width:100%\">\n", write );
	fputs( "\t<tr style=\"background-color:lightgrey\">\n", write );
	fputs( "\t\t<th>Roms</th>\n", write );
	fputs( "\t\t<th>Driver status</th>\n", write );
	fputs( "\t\t<th>Color</th>\n", write );
	fputs( "\t\t<th>Sound</th>\n", write );
	fputs( "\t\t<th>Samples</th>\n", write );
	fputs( "\t\t<th>Bios</th>\n", write );
	fputs( "\t</tr>\n", write );


	/***************** Search the DAT file line by line and process IDs *****************/
	while ( fgets(readline, MAXCHAR, read) != NULL )
	{
		char *target = NULL;
		char *start, *end;

		/***************** Read game tag *****************/
		if (( start = strstr( readline, game_id ) ))
		{
			realgame = 1;
			start += strlen( game_id );
			if (( end = strstr( start, "\"" ) ))
			{
				target = ( char * )malloc( end - start + 1 );
				memcpy( target, start, end - start );
				target[end - start] = '\0';

				strcpy( romname, target );
			}

			/***************** Check for sampleof *****************/
			if (( start = strstr( readline, sampleof ) ))
			{
				start += strlen( sampleof );
				if (( end = strstr( start, "\"" ) ))
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
		else if (( start = strstr( readline, driver_id ) ))
		{
			start += strlen( driver_id );
			if (( end = strstr( start, "\"" ) ))
			{
				target = ( char * )malloc( end - start + 1 );
				memcpy( target, start, end - start );
				target[end - start] = '\0';

				strcpy( driverstatus, target );
			}

			/***************** Check for color *****************/
			if (( start = strstr( readline, color ) ))
			{
				start += strlen( color );
				if (( end = strstr( start, "\"" ) ))
				{
					target = ( char * )malloc( end - start + 1 );
					memcpy( target, start, end - start );
					target[end - start] = '\0';

					strcpy( colorstatus, target );
				}
			}

			/***************** Check for sound *****************/
			if (( start = strstr( readline, sound ) ))
			{
				start += strlen( sound );
				if (( end = strstr( start, "\"" ) ))
				{
					target = ( char * )malloc( end - start + 1 );
					memcpy( target, start, end - start );
					target[end - start] = '\0';

					strcpy( soundstatus, target );
				}
			}
		}

		/***************** Read end game tag *****************/
		else if (( start = strstr( readline, endgame_id ) ))
		{
			/***************** Configure parent sample *****************/
			if ( parentsample && !clonesample ) strcpy( sampleused, romname );
			else if ( !parentsample && !clonesample ) strcpy( sampleused, "0" );

			/***************** Configure bios *****************/
			if ( bios ) strcpy( biosused, "1" );
			else strcpy( biosused, "0" );

			/***************** Write out html table data *****************/
			if ( realgame )
			{
				fputs( "\t<tr>\n", write );
				fputs( "\t\t<td>", write );
				fputs( romname, write );
				fputs( "</td>\n", write );

				if ( strcmp(driverstatus, "good") == 0 )
					fputs( "\t\t<td style=\"background-color:lightgreen\">", write );
				else
					fputs( "\t\t<td style=\"background-color:pink\">", write );
				fputs( driverstatus, write );
				fputs( "</td>\n", write );

				if ( strcmp(colorstatus, "good") == 0 )
					fputs( "\t\t<td style=\"background-color:lightgreen\">", write );
				else if ( strcmp(colorstatus, "imperfect") == 0 )
					fputs( "\t\t<td style=\"background-color:lightyellow\">", write );
				else fputs( "\t\t<td style=\"background-color:pink\">", write );
				fputs( colorstatus, write );
				fputs( "</td>\n", write );

				if ( strcmp(soundstatus, "good") == 0 )
					fputs( "\t\t<td style=\"background-color:lightgreen\">", write );
				else if ( strcmp(soundstatus, "imperfect") == 0 )
					fputs( "\t\t<td style=\"background-color:lightyellow\">", write );
				else fputs( "\t\t<td style=\"background-color:pink\">", write );
				fputs( soundstatus, write );
				fputs( "</td>\n", write );

				if ( strcmp(sampleused, "0") == 0 )
					fputs( "\t\t<td>", write );
				else
				{
					fputs( "\t\t<td>", write );
					fputs( sampleused, write );
				}
				fputs( "</td>\n", write );

				if ( strcmp(biosused, "0") == 0 )
					fputs( "\t\t<td>", write );
				else
					fputs( "\t\t<td>yes", write );
				fputs( "</td>\n", write );
				fputs( "\t</tr>\n", write );
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
	fputs( "</table>\n", write );
	fclose(read);
	fclose(write);

	/***************** Total games found *****************/
	printf("\nRoms found and processed:  %i\n\n", found);

	return 0;
}
