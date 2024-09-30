/*
	---- A Lempel-Ziv-Welch (LZW) Algorithm Implementation ----

	Filename:     LZWZ2.C
	Description:  An LZW implementation via hashing.
	
	We reserve code 256 as the END-of-FILE code (EOF_LZW_CODE).

	Variable-length lzwcodes; code size starts at 9 bits, then
	adds more bits as necessary.
	
	Usage:
	
		lzwz [-c[N]] [-r(+|-)] inputfile outputfile
	
	where N is bitsize of dictionary table size CODE_MAX. N is optional (default=16) 
	and N >= 12. After CODE_MAX+4K codes are transmitted, we reset the string table.

	Version 1.1 - Optional dictionary table size (9/21/2022); single file codec.
	Version 1.2 - Compression option to reset dictionary (12/09/2022).

	Note: -c24 -c25 -c26 are very slow on "-r-" for bigger files because you are outputting 
	big codes (24-bit, 25-bit, 26-bit), but somehow faster in -c27 and -c28 (why?). So not resetting 
	is not advisable but works on smaller files with more compression, but then again you can just 
	use a larger dictionary and set it to reset. Resetting means shorter codes are output again and 
	better adapts to the current segment of the file. Never mind.
	
	Gerald R. Tamayo, 2005/2009/2022/2023
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "utypes.h"
#include "gtbitio3.c"

#define EOF_LZW_CODE     256
#define LZW_NULL         256
#define START_LZW_CODE   257

#define output_code(a,b) put_nbits((a), (b))

enum {
	/* modes */
	COMPRESS = 1,
	DECOMPRESS,
};

typedef struct {
	char algorithm[4];
	int code_max_bits;
	int reset_dict;
} file_stamp;

/* code tables */
int *code;
int *prefix;
unsigned char *character;
unsigned char *stack_buffer, *stack=NULL;

int prefix_string_code = 0, lzw_code_cnt = 0;
int old_lzw_code = 0, new_lzw_code = 0, lzwcode;
int c = 0, code_max_bits = 16, /* default 65536 table size */
	code_MAX, hash_TABLE_SIZE, hash_SHIFT;
int reset_dict = 1; /* default = reset. */

int bit_count = 9;  /* code size starts at 9 bits. */
int code_max = 512; /* start expanding the code size if we
                         already reached this value. */

void copyright( void );
void compress_LZW( void );
void decompress_LZW( void );

/*
	initialize the codes with a value of LZW_NULL,
	indicating that the hash table slot is open.
*/
void init_code_table( void )
{
    int i;
    
    for ( i = 0; i < hash_TABLE_SIZE; i++ ) {
        code[ i ] = LZW_NULL;
    }
}

/*
	The insertion routine for the compressor, uses
	hashing to store the codes in the code tables.

	This function will always find an open slot, and
	when it does, it immediately exits the function.
*/
void insert_stringENC( int prefix_code, unsigned char c )
{
	int hindex;       /* the hashed index address. */
	int d;            /* the "displacement" to compute for the new index. */
	
	/* first probe is the hash function itself. */
	hindex = (c << hash_SHIFT) ^ prefix_code;

	/* prepare for the second probe. */
	d = hash_TABLE_SIZE - hindex;
	if ( hindex == 0 ) d = 1;

	do {
		/* available slot; store code here. */
		if ( code[ hindex ] == LZW_NULL ) {
			code[ hindex ] = lzw_code_cnt;
			prefix[ hindex ] = prefix_code;
			character[ hindex ] = c;
			return ;
		}
		/* otherwise, do a second probe. */
		if ( (hindex -= d) < 0 )
			hindex += hash_TABLE_SIZE;
	} while( 1 );
}

/*
	The decompression part does not actually need hashing,
	so just store the prefix codes and the append characters.
*/
void insert_stringDEC( int prefix_code, unsigned char c )
{
	prefix[ lzw_code_cnt ] = prefix_code;
	character[ lzw_code_cnt ] = c;
}

/*
	Search for the string composed of a prefix code
	and a character.
*/
int search_string( int prefix_code, unsigned char c )
{
	int hindex;       /* the hashed index address. */
	int d;            /* the "displacement" to compute for the new index. */
	
	hindex = (c << hash_SHIFT) ^ prefix_code;

	if ( hindex == 0 ) d = 1;
	else d = hash_TABLE_SIZE - hindex;
	
	do {
		/* empty slot; code pair not found. */
		if ( code[ hindex ] == LZW_NULL ) break;
		
		/* a code pair is stored here, so check it. */
		if (	prefix[ hindex ] == prefix_code
					&& character[ hindex ] == c ) { /* a match! */
			
			/* copy the code and return success. */
			prefix_string_code = code[ hindex ];
			return 1;
		}
		
		/* second probe; find another available slot. */
		if ( (hindex -= d) < 0 )
			hindex += hash_TABLE_SIZE;
	} while( 1 );
	
	return 0;
}

void usage( void )
{
    fprintf(stderr, "\n Usage: lzwz2 [-c[N]] [-nr] [-d] infile outfile");
    fprintf(stderr, "\n\n Options:\n\n  c[N] = compress, where N = bitsize of dictionary table size CODE_MAX (default=16); N=12..28.");
    fprintf(stderr, "\n  nr = compression option to not reset the dictionary dynamically, overall default=reset.");
	 fprintf(stderr, "\n         Note: Resetting is not advisable for bigger files; so very slow at bigger dictionary sizes.");
	 fprintf(stderr, "\n               Use lzwhc instead.");
	 fprintf(stderr, "\n  d = decompress.\n");
    copyright();
    exit (0);
}

int main( int argc, char *argv[] )
{
	float ratio = 0.0;
	file_stamp fstamp;
	int mode = -1, in_argn = 0, out_argn = 0, fcount = 0, n;
	
	clock_t start_time = clock();
	init_buffer_sizes( 1<<20 );
	
	/* command-line handler */
	if ( argc < 3 || argc > 5 ) usage();
	else if ( argc == 3 ) mode = COMPRESS;
	n = 1;
	while ( n < argc ){
		if ( argv[n][0] == '-' ){
			switch( tolower(argv[n][1]) ){
				case 'c':
					if ( argv[n][2] != 0 ){
						code_max_bits = atoi(&argv[n][2]);
						if ( code_max_bits < 12 ) usage();   /* smallest table size 4096 */
						else if ( code_max_bits > 28 ) usage();
					}
					if ( mode == DECOMPRESS ) usage();
					else mode = COMPRESS;
					break;
				case 'n':
					if ( argv[n][2] == 'r' ) reset_dict = 0;   /* no reset. */
					else usage();
					if ( mode == DECOMPRESS ) usage();
					else mode = COMPRESS;
					break;
				case 'd':
					if ( argv[n][2] != 0 || mode == COMPRESS ) usage();
					mode = DECOMPRESS;
					break;
				default: usage();
			}
		}
		else {
			if ( in_argn == 0 ) in_argn = n;
			else if ( out_argn == 0 ) out_argn = n;
			if ( ++fcount == 3 ) usage();
		}
		++n;
	}
	if ( in_argn == 0 || out_argn == 0 ) usage();
	
	/* Open input and output files. */
	if ( (gIN = fopen( argv[in_argn], "rb" )) == NULL ) {
		fprintf(stderr, "\nError opening input file, %s.", argv[in_argn] );
		return 0;
	}
	if ( (pOUT = fopen( argv[out_argn], "wb" )) == NULL ) {
		fprintf(stderr, "\nError opening output file, %s.", argv[out_argn] );
		return 0;
	}
	
	/* test file length. */
	if ( fgetc(gIN) == EOF ) return 0;  /* file length = 0. */
	else rewind(gIN);
	init_put_buffer();
	
	/* If DECOMPRESS mode, read input file and get code_max_bits. */
	if ( mode == DECOMPRESS ) {
		/* Read file stamp to get code_max_bits. */
		fread( &fstamp, sizeof(file_stamp), 1, gIN );
		code_max_bits = fstamp.code_max_bits;
		reset_dict = fstamp.reset_dict;
		init_get_buffer();
		nbytes_read = sizeof(file_stamp);
	}
	
	/* Set hash_TABLE_SIZE, hash_SHIFT, and code_MAX. */
	switch ( code_max_bits ) {
		case 12: hash_TABLE_SIZE =      5021; break;
		case 13: hash_TABLE_SIZE =      9859; break;
		case 14: hash_TABLE_SIZE =     18041; break;
		case 15: hash_TABLE_SIZE =     35023; break;
		case 16: hash_TABLE_SIZE =     69001; break;
		case 17: hash_TABLE_SIZE =    134989; break;
		case 18: hash_TABLE_SIZE =    279991; break;
		case 19: hash_TABLE_SIZE =    539881; break;
		case 20: hash_TABLE_SIZE =   1249943; break;
		case 21: hash_TABLE_SIZE =   2157151; break;
		case 22: hash_TABLE_SIZE =   4225303; break;
		case 23: hash_TABLE_SIZE =   8500249; break;
		case 24: hash_TABLE_SIZE =  16795123; break;
		case 25: hash_TABLE_SIZE =  33559021; break;
		case 26: hash_TABLE_SIZE =  67125433; break;
		case 27: hash_TABLE_SIZE = 134253857; break;
		case 28: hash_TABLE_SIZE = 268470641; break;
		default: break;
	}
	hash_SHIFT = code_max_bits - 8;
	code_MAX = 1 << code_max_bits;
	
	/* Allocate memory for the code tables. */
	if ( mode == COMPRESS ){
		code = (int *) malloc( sizeof(int) * hash_TABLE_SIZE );
		if ( !code ) {
			fprintf(stderr, "\n Error alloc: code buffer.");
			goto halt_prog;
		}
	}
	else if ( mode == DECOMPRESS ){
		/* allocate memory for the stack buffer. */
		stack_buffer = (unsigned char *) malloc( sizeof(unsigned char) * code_MAX );
		if ( !stack_buffer ) {
			fprintf(stderr, "\n Error alloc: stack buffer.");
			goto halt_prog;
		}
	}
	prefix = (int *) malloc( sizeof(int) * hash_TABLE_SIZE );
	if ( !prefix ) {
		fprintf(stderr, "\n Error alloc: prefix buffer.");
		goto halt_prog;
	}
	character = (unsigned char *) malloc( sizeof(unsigned char) * hash_TABLE_SIZE );
	if ( !character ) {
		fprintf(stderr, "\n Error alloc: character buffer.");
		goto halt_prog;
	}
	
	/* Finally, compress or decompress input file. */
	if ( mode == COMPRESS ){
		init_get_buffer();
		/* Write the FILE STAMP. */
		strcpy( fstamp.algorithm, "LZW" );
		fstamp.code_max_bits = code_max_bits;
		fstamp.reset_dict = reset_dict;
		fwrite( &fstamp, sizeof(file_stamp), 1, pOUT );
		nbytes_out = sizeof(file_stamp);
		
		fprintf(stderr, "\nDictionary size used   = %15lu codes", (ulong) code_MAX );
		
		fprintf(stderr, "\n\nLZW Encoding [ %s to %s ] ...", argv[in_argn], argv[out_argn] );
		compress_LZW();
	}
	else if ( mode == DECOMPRESS ){
		fprintf(stderr, "\nLZW Decoding...");
		decompress_LZW();
	}
	flush_put_buffer();
	nbytes_read = get_nbytes_read();
	
	fprintf(stderr, "done.\n %s (%lld) -> %s (%lld)", argv[in_argn], nbytes_read, argv[out_argn], nbytes_out);
	if ( mode == COMPRESS ) {
		ratio = (((float) nbytes_read - (float) nbytes_out) / (float) nbytes_read ) * (float) 100;
		fprintf(stderr, "\nCompression ratio: %3.2f %%", ratio );
	}
	
	halt_prog:
	
	free_put_buffer();
	free_get_buffer();
	if ( code ) free( code );
	if ( prefix ) free( prefix );
	if ( character ) free( character );
	if ( stack_buffer ) free( stack_buffer );
	fclose( gIN );
	fclose( pOUT );
	
	if ( mode == DECOMPRESS ) nbytes_read = nbytes_out;
	fprintf(stderr, " in %3.2f secs (@ %3.2f MB/s)\n",
		(double)(clock()-start_time)/CLOCKS_PER_SEC, (nbytes_read/1048576)/((double)(clock()-start_time)/CLOCKS_PER_SEC) );
	return 0;
}

void copyright( void )
{
	fprintf(stderr, "\n :: Gerald R. Tamayo (c) 2005-2023\n");
}

void compress_LZW( void )
{
	/* initialize the LZW code table. */
	init_code_table();
	
	/* start of codes to define. */
	lzw_code_cnt = START_LZW_CODE;
	
	/* get first character. */
	if ( (c=gfgetc()) != EOF ) {
		prefix_string_code = c;	/* first prefix code. */
	}
	
	while( (c=gfgetc()) != EOF ) {
		if ( !search_string(prefix_string_code, c) ) {
			output_code ( (unsigned int) prefix_string_code, bit_count );
			
			/* ---- insert the string in the string table. ---- */
			if ( lzw_code_cnt < code_MAX ){
				insert_stringENC( prefix_string_code, c );
				if ( lzw_code_cnt == code_max ) {
					bit_count++;
					code_max <<= 1;
				}
			}
			
			/*  Instead of monitoring comp. ratio, we simply reset 
				the string table after N output codes. 
				No CLEAR_TABLE code is transmitted. */
			if ( reset_dict == 1 ){
				if ( lzw_code_cnt <= (code_MAX+4096) && lzw_code_cnt++ == (code_MAX+4096) ) {
					init_code_table();
					lzw_code_cnt = START_LZW_CODE;
					bit_count =   9;
					code_max  = 512;
				}
			}
			else if ( reset_dict == 0 ){
				if ( lzw_code_cnt < code_MAX ) ++lzw_code_cnt;
			}
			
			/* string = char */
			prefix_string_code = c;
		}
	}
	/* output last code. */
	output_code ( (unsigned int) prefix_string_code, bit_count );
	
	/* output END-of-FILE code.*/
	output_code ( (unsigned int) EOF_LZW_CODE, bit_count );
}

void decompress_LZW( void )
{
	/* set the starting code to define. */
	lzw_code_cnt = START_LZW_CODE;
	
	/* get first code. */
	old_lzw_code = get_nbits( bit_count );
	
	/* first code is a character; output it. */
	pfputc( (unsigned char) old_lzw_code );
	
	while ( 1 ) {
		new_lzw_code = get_nbits( bit_count );
		
		if ( new_lzw_code == EOF_LZW_CODE ) break;
		else if ( new_lzw_code >= lzw_code_cnt ) lzwcode = old_lzw_code;
		else lzwcode = new_lzw_code;
		
		/* GET STRING/PATTERN. */
		stack = stack_buffer;
		while ( lzwcode > EOF_LZW_CODE ) {
			*stack++ = character[ lzwcode ];
			/* when loop exits, lzwcode must be a character. */
			lzwcode = prefix[ lzwcode ];
		}
		
		/* K = get first character of string. */
		*stack = lzwcode;
		
		/* OUTPUT STRING/PATTERN. */
		while ( stack >= stack_buffer ) {
			pfputc( *stack-- );
		}
		/* if undefined code. */
		if ( new_lzw_code >= lzw_code_cnt ) {
			pfputc( lzwcode );
		}

		/* add PREV_CODE+K to the string table. */
		if ( lzw_code_cnt < code_MAX ) {
			insert_stringDEC( old_lzw_code, (unsigned char) lzwcode );
			if ( bit_count < code_max_bits ){
				if ( lzw_code_cnt == (code_max-1) ) {
					bit_count++;
					code_max <<= 1;
				}
			}
		}
		
		/* PREV_CODE = CURR_CODE */
		old_lzw_code = new_lzw_code;

		if ( reset_dict == 1 ){
			/* reset table if number of codes transmitted reach (code_MAX+4K) */
			if ( ++lzw_code_cnt == (code_MAX+4096) ) {
				lzw_code_cnt = START_LZW_CODE;
				bit_count =   9;
				code_max  = 512;
				
				/* get first code. */
				old_lzw_code = get_nbits( bit_count );
				
				/* first code is a character; output it. */
				pfputc( (unsigned char) old_lzw_code );
			}
		}
		else if ( reset_dict == 0 ){
			if ( lzw_code_cnt < code_MAX ) ++lzw_code_cnt;
		}
	}
}
