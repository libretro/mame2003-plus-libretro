/*

	Beta version 1  -  Jan. 9th 2021
	by: mahoneyt944 - MAME 2003-Plus Team.

	notes: - currently writes to a log, will work on writing to a html table next.
	       - one issue is present so far, parent roms that use samples are listed as nosampleof
	         because we scan the game tag for the sample name and parents do not list this here.
	         Will probably have to scan for the actual sample.wav lines if sampleof is not found
	         first and end if it reaches a closing tag.

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXCHAR 200

void banner(void)
{
	printf("\n'########:::::'###::::'########:\n");
	printf(" ##.... ##:::'## ##:::... ##..::\n");
	printf(" ##:::: ##::'##:. ##::::: ##::::\n");
	printf(" ##:::: ##:'##:::. ##:::: ##::::\n");
	printf(" ##:::: ##: #########:::: ##::::\n");
	printf(" ##:::: ##: ##.... ##:::: ##::::\n");
	printf(" ########:: ##:::: ##:::: ##::::\n");
	printf("........:::..:::::..:::::..:::::\n");
	printf("'##::::'##::::'###:::::'######:::'####::'######::\n");
	printf(" ###::'###:::'## ##:::'##... ##::. ##::'##... ##:\n");
	printf(" ####'####::'##:. ##:: ##:::..:::: ##:: ##:::..::\n");
	printf(" ## ### ##:'##:::. ##: ##::'####:: ##:: ##:::::::\n");
	printf(" ##. #: ##: #########: ##::: ##::: ##:: ##:::::::\n");
	printf(" ##:.:: ##: ##.... ##: ##::: ##::: ##:: ##::: ##:\n");
	printf(" ##:::: ##: ##:::: ##:. ######:::'####:. ######::\n");
	printf("..:::::..::..:::::..:::......::::....:::......:::\n");
}

int main()
{
	banner();
	char* dat = "mame2003-plus.xml";
	FILE *read;
	FILE *write;
	char readline[MAXCHAR];

	char game_id[] = "<game name=\"";
	char driver_id[] = "<driver status=\"";

	char sampleof[] = "sampleof=\"";
	char color[] = "color=\"";
	char sound[] = "sound=\"";

	int found = 0;

	/***************** try to open the DAT file *****************/
	read = fopen(dat, "r");
	printf("\nTry to open the DAT file:  %s\n", dat);

	if (read == NULL)
	{
		printf("Could not open DAT file :  %s not found in directory.\n\n", dat);
		return 1;
	}

	/***************** open log to write to *****************/
	write = fopen("log.txt", "w");
	printf("\nProcessing DAT now.\n");


	/***************** Search the DAT file line by line for ids *****************/
	while ( fgets(readline, MAXCHAR, read) != NULL )
	{
		char *target = NULL;
		char *start, *end;

		/***************** Read game tag *****************/
		if ( start = strstr( readline, game_id ) )
		{
			start += strlen( game_id );
			if ( end = strstr( start, "\"" ) )
			{
				target = ( char * )malloc( end - start + 1 );
				memcpy( target, start, end - start );
				target[end - start] = '\0';

				fputs(target, write);
				fputs(":", write);
				free(target);
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

					fputs(target, write);
					fputs(":", write);
					free(target);
				}
			}
			else fputs("nosampleof:", write);

			found++;
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

				fputs(target, write);
				fputs(":", write);
				free(target);
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

					fputs(target, write);
					fputs(":", write);
					free(target);
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

					fputs(target, write);
					fputs("\n", write);
					free(target);
				}
			}
		}

	}

	/* close up our files */
	printf("Closing DAT file.\n");
	fclose(read);
	fclose(write);

	/* Total games found*/
	printf("\nRoms found and processed:  %i\n\n", found);

	return 0;
}
