#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void help(char * prname)
{
	fprintf(stderr, "USE: %s <n><wordsizeinbits>\n", prname);
	exit(1);
}

int main(int argc, char ** argv)
{
	if(argc != 3) help(argv[0]);
	int n = atoi(argv[1]);
	int wordsize = (atoi(argv[2]) >> 1);
	int i, j, n_assigned;
	for(i = 0; i < n; i++)
	{
		int num = i;
		n_assigned = 0;
		for(j = 0; j < wordsize; j++)
		{			
			if ((num & 0x0003) != 3) 
			{
				n_assigned++;
				//fprintf(stderr, "num:%d\n", num);
			}
			num = num >> 2;
		}
		if(i%16 == 0) fprintf(stderr, "\n");
		fprintf(stderr, "%d, ", n_assigned);	
	} 
	fprintf(stderr, "\n");
}
