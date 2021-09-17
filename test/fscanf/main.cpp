#include <stdio.h>
#include <string.h>

FILE* f;
char  def[80];
char  strparm[100];
char  boomcfgline[100];

unsigned int test, lndef, lncfg;

void main()
{
    f = fopen("boom.cfg", "r");
    
    if (f)
    {
            
        while (!feof(f))
        {
            //test = fscanf (f, "%79s %[^\n]\n", def, strparm);
            ////test = sscanf("# Doom config file\n# Format:\n", "%79s %[^\n]\n", def, strparm);
            //printf("%d  ", test);      // ptek
            //printf("%s\n", def);      // ptek
            //printf("%s\n", strparm);      // ptek


            if( fgets(boomcfgline, 100, f) == NULL)
            {
                printf( "fgets error\n" );
                //exit(0);
            }
            
            printf("%s", boomcfgline);

			// remove linefeed from the line if any
			lncfg = strlen(boomcfgline);
			if (boomcfgline[lncfg-1] == 10)
			{
				boomcfgline[lncfg-1] = 0;
				lncfg--;
			}

            // get def
            test = sscanf (boomcfgline, "%79s", def);
            if (test == -1)
                printf("got linefeed\n");
            else
            {
                // get strparm
                lndef = strlen(def);

                if (lndef != lncfg)
                {
                    strcpy(strparm, boomcfgline + lndef);
					test = 2;
                }
            }

        }

        fclose (f);
    }

}
