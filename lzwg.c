/*
	---- A Lempel-Ziv-Welch (LZW) Algorithm Implementation ----

	Filename:     LZWG.C
	Description:  An LZW implementation using a Binary Search Tree.
	Author:       Gerald Tamayo, 2005/2009/2010/2022/2023
	
	Decompression program:	LZWGD.C

	We reserve code 256 as the END-of-FILE code (EOF_LZW_CODE).

	*Variable-length* lzwcodes; code size starts at 9 bits, then
	adds more bits as necessary.
	
	Usage:
	
		lzwg [-N] inputfile outputfile
	
	where N is bitsize of dictionary table size CODE_MAX. N is >= 12.
	After CODE_MAX+4K codes are transmitted, we reset the string table.

	Version 1.1 - Optional dictionary table size (9/14/2022).
	
	Gerald R. Tamayo, 2005/2009/2022
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utypes.h"
#include "gtbitio2.c"
#include "lzwbt.c"

#define CODE_MAX_BITS     16   /* default */

#define EOF_LZW_CODE     256
#define START_LZW_CODE   257

#define output_code(a,b) put_nbits((a), (b))

typedef struct {
	char algorithm[4];
	int code_max_bits;
} file_stamp;

int code_MAX = (1<<CODE_MAX_BITS);   /* STRING_TABLE_SIZE, default = 65536 */

int bit_count =   9;  /* code size starts at 9 bits. */
int code_max  = 512;  /* start expanding the code size if we
                           already reached this value. */

void copyright( void );

void usage( void )
{
	fprintf(stderr, "\n Usage: lzwg [-N] infile outfile");
	fprintf(stderr, "\n\n where N = bitsize of table size CODE_MAX (default=16); N >= 12.");
	copyright();
	exit (0);
}

int main( int argc, char *argv[] )
{
	float ratio = 0.0;
	int c = 0, code_max_bits = CODE_MAX_BITS;
	int in_argn = 1, out_argn = 2;
	file_stamp fstamp;
	
	clock_t start_time = clock();
	
	if ( argc == 4 ) {
		if ( argv[1][0] == '-' && argv[1][1] != '\0' ) {
			code_max_bits = atoi(&argv[1][1]);
			if ( code_max_bits == 0 ) usage();
			else if ( code_max_bits < 12 ) {
				code_max_bits = 12;   /* smallest table size = 4096  */
			}
		}
		else usage();
		code_MAX = (1<<code_max_bits);
	}
	
	if ( argc == 3 ){
		in_argn = 1;
		out_argn = 2;
	}
	else if ( argc == 4 ){
		in_argn = 2;
		out_argn = 3;
	}
	else usage();
	
	if ( (gIN = fopen( argv[in_argn], "rb" )) == NULL ) {
		fprintf(stderr, "\nError opening input file.");
		return 0;
	}
	if ( (pOUT = fopen( argv[out_argn], "wb" )) == NULL ) {
		fprintf(stderr, "\nError opening output file.");
		goto halt_prog;
	}
	init_buffer_sizes(1<<15);
	init_get_buffer();
	init_put_buffer();
	
	/* allocate and initialize the code tables. */
	if ( !alloc_code_tables(code_MAX, LZW_COMPRESS) ) {
		fprintf( stderr, "\nError alloc!");
		goto halt_prog;
	}
	init_code_tables(code_MAX, LZW_COMPRESS);
	
	fprintf(stderr, "\n--[ A Lempel-Ziv-Welch (LZW) Implementation ]--");
	fprintf(stderr, "\n\nDictionary size used     = %15lu codes", (ulong) code_MAX );
	fprintf(stderr, "\n\nName of input file : %s", argv[in_argn] );
	
	/* Start compressing to output file. */
	fprintf(stderr, "\n Compressing...");
	
	if ( nfread == 0 ) goto done_compression;  /* file length = 0 */
	
	/* Write the FILE STAMP. */
	strcpy( fstamp.algorithm, "LZW" );
	fstamp.code_max_bits = code_max_bits;
	fwrite( &fstamp, sizeof(file_stamp), 1, pOUT );
	nbytes_out = sizeof(file_stamp);
	
	/* start of codes to define. */
	lzw_code_cnt = START_LZW_CODE;
	
	/* get first character. */
	if ( (c=gfgetc()) != EOF ){
		prefix_string_code = c;	/* first prefix code. */
	}
	
	while( (c=gfgetc()) != EOF ) {
		if ( !lzw_search(prefix_string_code, c) ) {
			output_code ( (unsigned int) prefix_string_code, bit_count );
			
			/* ---- insert the string in the string table. ---- */
			if ( lzw_code_cnt < code_MAX ){
				lzw_comp_insert( prefix_string_code, c );
				if ( lzw_code_cnt == code_max ) {
					bit_count++;
					code_max <<= 1;
				}
			}
			
			/*  Instead of monitoring comp. ratio, we simply reset 
				the string table after code_MAX+4K output codes. 
				No CLEAR_TABLE code is transmitted. */
			if ( lzw_code_cnt++ == (code_MAX+4096) ) {
				init_code_tables(code_MAX, LZW_COMPRESS);
				lzw_code_cnt = START_LZW_CODE;
				bit_count =   9;
				code_max  = 512;
			}
			
			/* string = char */
			prefix_string_code = c;
		}
	}
	output_code ( (unsigned int) prefix_string_code, bit_count );
	
	/* output END-of-FILE code.*/
	output_code ( (unsigned int) EOF_LZW_CODE, bit_count );
	
	flush_put_buffer();
	
	done_compression:
	
	fprintf(stderr, " complete.");
	
	/* get infile's size and get compression ratio. */
	nbytes_read = get_nbytes_read();
	
	fprintf(stderr, "\nName of output file: %s", argv[ out_argn ] );
	fprintf(stderr, "\nLength of input file     = %15llu bytes", nbytes_read );
	fprintf(stderr, "\nLength of output file    = %15llu bytes", nbytes_out );
	
	ratio = (nbytes_read==0? 0:((((float) nbytes_read - (float) nbytes_out)
						/ (float) nbytes_read ) * (float) 100));

	fprintf(stderr, "\nCompression ratio:         %15.2f %% in %3.2f secs.\n", ratio, 
		(double) (clock()-start_time) / CLOCKS_PER_SEC );

	halt_prog:
	
	free_put_buffer();
	free_get_buffer();
	free_code_tables();
	if ( gIN ) fclose( gIN );
	if ( pOUT ) fclose( pOUT );
	return 0;
}

void copyright( void )
{
	fprintf(stderr, "\n\n :: lzwg file compressor, Gerald R. Tamayo, 2005/2023\n");
}
