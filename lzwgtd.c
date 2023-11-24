/*
	---- A Lempel-Ziv-Welch (LZW) Algorithm Implementation ----

	Filename:  LZWGTD.C	(the decoder to LZWGT.C)
	Author:    Gerald R. Tamayo, 2005/2008/2010/2022/2023
*/
#include <stdio.h>
#include <stdlib.h>
#include "utypes.h"
#include "gtbitio2.c"
#include "lzwbt.c"

#define CODE_MAX_BITS     16
#define CODE_MAX        (1<<CODE_MAX_BITS)

#define EOF_LZW_CODE     256
#define START_LZW_CODE   257

#define get_code() get_nbits( bit_count )

typedef struct {
	char algorithm[4];
	int N;
} file_stamp;

int bit_count = 9;  /* code size starts at 9 bits. */
int code_max = 512; /* start expanding the code size if we
                         already reached this value. */

unsigned char stack_buffer[ CODE_MAX ], *stack=NULL;

void copyright( void );

int main( int argc, char *argv[] )
{
	file_stamp fstamp;
	int old_lzw_code = 0, new_lzw_code = 0, lzwcode;
	int N;
	
	if ( argc != 3 ) {
		fprintf(stderr, "\n Usage: lzwgtd infile outfile");
		copyright();
		return 0;
	}
	if ( (gIN = fopen( argv[1], "rb")) == NULL ) {
		fprintf(stderr, "\nError opening input file.");
		return 0;
	}
	if ( (pOUT = fopen( argv[2], "wb" )) == NULL ) {
		fprintf(stderr, "\nError opening output file.");
		return 0;
	}
	init_buffer_sizes(1<<15);
	init_put_buffer();
	
	fprintf(stderr, "\nName of input file : %s", argv[1] );
	
	/* start deCompressing to output file. */
	fprintf(stderr, "\n Decompressing...");

	/* read the file header, */
	fread( &fstamp, sizeof( file_stamp ), 1, gIN );
	
	/* and initialize the input buffer. */
	init_get_buffer();
	if ( nfread == 0 ) goto done_decompression;
	
	N = fstamp.N;
	
	/* allocate and initialize the code tables. */
	if ( !alloc_code_tables(CODE_MAX, LZW_DECOMPRESS) ) {
		fprintf( stderr, "\nError alloc!");
		goto halt_prog;
	}
	init_code_tables(CODE_MAX, LZW_DECOMPRESS);
	
	/* set the starting code to define. */
	lzw_code_cnt = START_LZW_CODE;
	
	/* get first code. */
	old_lzw_code = get_nbits( bit_count );
	
	/* first code is a character; output it. */
	pfputc( (unsigned char) old_lzw_code );
	
	while( 1 ) {
		new_lzw_code = get_nbits( bit_count );
		
		if ( new_lzw_code == EOF_LZW_CODE ) break;
		else if ( new_lzw_code >= lzw_code_cnt ) lzwcode = old_lzw_code;
		else lzwcode = new_lzw_code;
		
		/* GET STRING/PATTERN. */
		stack = stack_buffer;
		while ( lzwcode > EOF_LZW_CODE ) {
			*stack++ = code_char[ lzwcode ];
			/* when loop exits, lzwcode must be a character. */
			lzwcode = code_prefix[ lzwcode ];
		}
		
		/* K = get first character of string. */
		*stack = lzwcode;
		
		/* OUTPUT STRING/PATTERN. */
		while( stack >= stack_buffer ) {
			pfputc( *stack-- );
		}
		/* if undefined code. */
		if ( new_lzw_code >= lzw_code_cnt ) {
			pfputc( lzwcode );
		}
		
		/* add PREV_CODE+K to the string table. */
		if ( lzw_code_cnt < CODE_MAX ) {
			lzw_decomp_insert( old_lzw_code, (unsigned char) lzwcode );
			if ( bit_count < CODE_MAX_BITS ){
				if ( lzw_code_cnt == (code_max-1) ) {
					bit_count++;
					code_max <<= 1;
				}
			}
		}
		
		/* PREV_CODE = CURR_CODE */
		old_lzw_code = new_lzw_code;
		
		/* reset table. */
		if ( ++lzw_code_cnt == N ) {
			lzw_code_cnt = START_LZW_CODE;
			bit_count =   9;
			code_max  = 512;
			
			/* get first code. */
			old_lzw_code = get_nbits( bit_count );
			
			/* first code is a character; output it. */
			pfputc( (unsigned char) old_lzw_code );
		}
	}
	flush_put_buffer();
	
	done_decompression:
	
	fprintf(stderr, "done.");
	fprintf(stderr, "\nName of output file: %s\n", argv[2] );
	
	halt_prog:
	
	free_get_buffer();
	free_put_buffer();
	free_code_tables();
	if ( gIN ) fclose( gIN );
	if ( pOUT ) fclose( pOUT );
	return 0;
}

void copyright( void )
{
	fprintf(stderr, "\n\n:: Gerald R. Tamayo, 2005/2023\n");
}
