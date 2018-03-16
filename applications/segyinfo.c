#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <segyio/segy.h>

static void printSegyTraceInfo( const char* buf ) {
    int cdp, tsf, xl, il;
    segy_get_field( buf, SEGY_TR_ENSEMBLE, &cdp );
    segy_get_field( buf, SEGY_TR_SEQ_FILE, &tsf );
    segy_get_field( buf, SEGY_TR_CROSSLINE, &xl );
    segy_get_field( buf, SEGY_TR_INLINE, &il );

    printf("cdp:               %d\n", cdp );
    printf("TraceSequenceFile: %d\n", tsf );
    printf("Crossline3D:       %d\n", xl );
    printf("Inline3D:          %d\n", il );
}

#define minimum(x,y) ((x) < (y) ? (x) : (y))
#define maximum(x,y) ((x) > (y) ? (x) : (y))

typedef union {
    int32_t i32val;
    int16_t i16val;
    int8_t i8val;
    float fval;
} MutableTraceType ;

int compare_assign_max(MutableTraceType* val, void* trace, int format) {
    switch( format ) {
        case SEGY_IBM_FLOAT_4_BYTE:
        case SEGY_IEEE_FLOAT_4_BYTE:
            if (val->fval < *((float*)trace)) {
                val->fval = *((float*)trace) ;
                return 1 ;
            }
            break;
            
        case SEGY_SIGNED_INTEGER_4_BYTE:
            if (val->i32val < *((int32_t*)trace)) {
                val->i32val = *((int32_t*)trace) ;
                return 1 ;
            }
            break;
            
        case SEGY_SIGNED_SHORT_2_BYTE:
            if (val->i16val < *((int16_t*)trace)) {
                val->i16val = *((int16_t*)trace) ;
                return 1 ;
            }
            break;
            
        case SEGY_SIGNED_CHAR_1_BYTE:
            if (val->i8val < *((int8_t*)trace)) {
                val->i8val = *((int8_t*)trace) ;
                return 1 ;
            }
    }
    
    return 0 ;
}

bool compare_assign_min(MutableTraceType* val, void* trace, int format) {
    switch( format ) {
        case SEGY_IBM_FLOAT_4_BYTE:
        case SEGY_IEEE_FLOAT_4_BYTE:
            if (val->fval > *((float*)trace)) {
                val->fval = *((float*)trace) ;
                return true ;
            }
            break;
            
        case SEGY_SIGNED_INTEGER_4_BYTE:
            if (val->i32val > *((int32_t*)trace)) {
                val->i32val = *((int32_t*)trace) ;
                return true ;
            }
            break;
            
        case SEGY_SIGNED_SHORT_2_BYTE:
            if (val->i16val > *((int16_t*)trace)) {
                val->i16val = *((int16_t*)trace) ;
                return true ;
            }
            break;
            
        case SEGY_SIGNED_CHAR_1_BYTE:
            if (val->i8val > *((int8_t*)trace)) {
                val->i8val = *((int8_t*)trace) ;
                return true ;
            }
            break;
    }
    
    return false ;
}


int main(int argc, char* argv[]) {
    
    if( argc < 2 ) {
        puts("Missing argument, expected run signature:");
        printf("  %s <segy_file> [mmap]\n", argv[0]);
        exit(1);
    }

    segy_file* fp = segy_open( argv[ 1 ], "rb" );
    if( !fp ) {
        perror( "fopen():" );
        exit( 3 );
    }

    if( argc > 2 && strcmp( argv[ 2 ], "mmap" ) == 0 ) {
        int err = segy_mmap( fp );
        if( err != SEGY_OK )
            fputs( "Could not mmap file. Using fstream fallback.", stderr );
    }

    int err;
    char header[ SEGY_BINARY_HEADER_SIZE ];
    err = segy_binheader( fp, header );

    if( err != 0 ) {
        perror( "Unable to read segy binary header:" );
        exit( err );
    }

    const int format = segy_format( header );
    const int samples = segy_samples( header );
    const long trace0 = segy_trace0( header );
    const int nbytes_sample = segy_sample_nbytes( header ) ;
    const int trace_bsize = nbytes_sample * samples ;
    int extended_headers;
    err = segy_get_bfield( header, SEGY_BIN_EXT_HEADERS, &extended_headers );

    if( err != 0 ) {
        perror( "Can't read 'extended headers' field from binary header" );
        exit( err );
    }

    int traces;
    err = segy_traces( fp, &traces, trace0, trace_bsize );

    if( err != 0 ) {
        perror( "Could not determine traces" );
        exit( err );
    }

    printf( "Sample format: %d\n", format );
    printf( "Samples per trace: %d\n", samples );
    printf( "Bytes per sample: %d\n", nbytes_sample );
    printf( "Traces: %d\n", traces );
    printf("Extended text header count: %d\n", extended_headers );
    puts("");


    char traceh[ SEGY_TRACE_HEADER_SIZE ];
    err = segy_traceheader( fp, 0, traceh, trace0, trace_bsize );
    if( err != 0 ) {
        perror( "Unable to read trace 0:" );
        exit( err );
    }

    puts("Info from first trace:");
    printSegyTraceInfo( traceh );

    err = segy_traceheader( fp, 1, traceh, trace0, trace_bsize );
    if( err != 0 ) {
        perror( "Unable to read trace 1:" );
        exit( err );
    }

    puts("");
    puts("Info from second trace:");
    printSegyTraceInfo( traceh );

    clock_t start = clock();
    void* trbuf = malloc( trace_bsize );

    MutableTraceType minval;
    MutableTraceType maxval;
    int minval_trace = -1;
    int maxval_trace = -1;
    
    switch( format ) {
        case SEGY_IBM_FLOAT_4_BYTE:
            printf("Sample type: SEGY_IBM_FLOAT_4_BYTE\n");
            minval.fval = 1e20 ;
            maxval.fval = -1e20 ;
            break;
            
        case SEGY_SIGNED_INTEGER_4_BYTE:
            printf("Sample type: SEGY_SIGNED_INTEGER_4_BYTE\n");
            minval.i32val = INT32_MAX ;
            maxval.i32val = INT32_MIN ;
            break;
            
        case SEGY_SIGNED_SHORT_2_BYTE:
            printf("Sample type: SEGY_SIGNED_SHORT_2_BYTE\n");
            minval.i16val = 999 ;
            maxval.i16val = -999 ;
            break;
            
            
        case SEGY_IEEE_FLOAT_4_BYTE:
            printf("Sample type: SEGY_IEEE_FLOAT_4_BYTE\n");
            minval.fval = FLT_MAX ;
            maxval.fval = FLT_MIN ;
            break;
            
        case SEGY_FIXED_POINT_WITH_GAIN_4_BYTE:
        case SEGY_NOT_IN_USE_1:
        case SEGY_NOT_IN_USE_2:
            printf("Unsupported sample format: %d\n", format);
            break;
            
        case SEGY_SIGNED_CHAR_1_BYTE:
            printf("Sample type: SEGY_SIGNED_CHAR_1_BYT\n");
            minval.i8val = INT8_MAX ;
            maxval.i8val = INT8_MIN ;
            break;
    }

    int min_sample_count = 999999999;
    int max_sample_count = 0;
    for( int i = 0; i < traces; ++i ) {
        err = segy_traceheader( fp, i, traceh, trace0, trace_bsize );
        if( err != 0 ) {
            perror( "Unable to read trace" );
            exit( err );
        }

        int sample_count;
        err = segy_get_field( traceh, SEGY_TR_SAMPLE_COUNT, &sample_count );

        if( err != 0 ) {
            fprintf( stderr, "Invalid trace header field: %d\n", SEGY_TR_SAMPLE_COUNT );
            exit( err );
        }

        min_sample_count = minimum( sample_count, min_sample_count );
        max_sample_count = maximum( sample_count, max_sample_count );

        err = segy_readtrace( fp, i, trbuf, trace0, trace_bsize, nbytes_sample );

        if( err != 0 ) {
            fprintf( stderr, "Unable to read trace: %d\n", i );
            exit( err );
        }

        segy_to_native( format, samples, trbuf );

        for( int j = 0; j < samples; ++j ) {
            if ( compare_assign_min(&minval, (char*)trbuf+j*nbytes_sample, format) ) {
                minval_trace = i ;
            }
            
            if (compare_assign_max(&maxval, (char*)trbuf+j*nbytes_sample, format) ) {
                maxval_trace = i ;
            }
        }
    }

    free( trbuf );

    puts("");
    puts("Info from last trace:");
    err = segy_traceheader( fp, traces - 1, traceh, trace0, trace_bsize );

    if( err != 0 ) {
        perror( "Unable to read trace." );
        exit( err );
    }

    printSegyTraceInfo( traceh );

    puts("");
    printf("Min sample count: %d\n", min_sample_count);
    printf("Max sample count: %d\n", max_sample_count);
    
    switch( format ) {
        case SEGY_IBM_FLOAT_4_BYTE:
        case SEGY_IEEE_FLOAT_4_BYTE:
            printf("Min sample value: %f at trace %d\n", minval.fval, minval_trace );
            printf("Max sample value: %f at trace %d\n", maxval.fval, maxval_trace );
            break;
            
        case SEGY_SIGNED_INTEGER_4_BYTE:
            printf("Min sample value: %d at trace %d\n", minval.i32val, minval_trace );
            printf("Max sample value: %d at trace %d\n", maxval.i32val, maxval_trace );
            break;
            
        case SEGY_SIGNED_SHORT_2_BYTE:
            printf("Min sample value: %d at trace %d\n", minval.i16val, minval_trace );
            printf("Max sample value: %d at trace %d\n", maxval.i16val, maxval_trace );
            break;
            
        case SEGY_SIGNED_CHAR_1_BYTE:
            printf("Min sample value: %d at trace %d\n", minval.i8val, minval_trace );
            printf("Max sample value: %d at trace %d\n", maxval.i8val, maxval_trace );
            break;
    }
    
    puts("");

    clock_t diff = clock() - start;
    printf("Read all trace headers in: %.2f s\n", (double) diff / CLOCKS_PER_SEC);

    segy_close( fp );
    return 0;
}
