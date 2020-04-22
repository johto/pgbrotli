#include "postgres.h"
#include "funcapi.h"
#include "fmgr.h"
#include "utils/builtins.h"

#include <brotli/encode.h>
#include <brotli/decode.h>

/* We refuse to work with values that are too large for practical reasons. */
#define MAX_VALUE_LENGTH 1073741824

PG_MODULE_MAGIC;


static void*
pgbrotli_alloc(void *opaque, size_t size)
{
	return palloc(size);
}

static void
pgbrotli_free(void *opaque, void *address)
{
	if (address == NULL)
		return;
	return pfree(address);
}


Datum pgbrotli_compress_text(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pgbrotli_compress_text);

Datum
pgbrotli_compress_text(PG_FUNCTION_ARGS)
{
	text *input_text = PG_GETARG_TEXT_PP(0);
	char *input = text_to_cstring(input_text);
	size_t input_size = VARSIZE_ANY_EXHDR(input_text);
	bytea *output;
	size_t output_size;

	if (input_size > MAX_VALUE_LENGTH)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("input value exceeds maximum length %zu", input_size)));

	output_size = BrotliEncoderMaxCompressedSize(input_size);
	if (output_size == 0)
	{
		/*
		 * Shouldn't happen unless input is above 2GB, which ought to be
		 * impossible while we're inside postgres.
		 */
		elog(ERROR, "internal error: BrotliEncoderMaxCompressedSize returned 0");
	}
	else if (output_size > MAX_VALUE_LENGTH)
	{
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("required buffer space %zu exceeds maximum length %u",
						output_size, MAX_VALUE_LENGTH)));
	}
	output = (bytea *) palloc(VARHDRSZ + output_size);

	BrotliEncoderState* enc = BrotliEncoderCreateInstance(pgbrotli_alloc, pgbrotli_free, NULL);
	if (enc == NULL)
		elog(ERROR, "could not initialize compression state");

	if (BrotliEncoderCompress(BROTLI_MAX_QUALITY, BROTLI_DEFAULT_WINDOW, BROTLI_MODE_TEXT,
							  input_size, (const uint8_t *) input,
							  &output_size, (uint8_t *) VARDATA(output)) != BROTLI_TRUE)
	{
		BrotliEncoderDestroyInstance(enc);
		elog(ERROR, "compression fail");
	}
	BrotliEncoderDestroyInstance(enc);
	SET_VARSIZE(output, VARHDRSZ + output_size);
	PG_RETURN_BYTEA_P(output);
}

Datum pgbrotli_compress_bytea(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pgbrotli_compress_bytea);

Datum
pgbrotli_compress_bytea(PG_FUNCTION_ARGS)
{
	elog(ERROR, "not implemented yet");
}

Datum pgbrotli_decompress(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pgbrotli_decompress);

Datum
pgbrotli_decompress(PG_FUNCTION_ARGS)
{
	bytea *input = PG_GETARG_BYTEA_PP(0);
	size_t input_size = VARSIZE_ANY_EXHDR(input);
	size_t available_in;
	uint8_t *next_in = (uint8_t *) VARDATA_ANY(input);
	bytea *output;
	size_t output_allocated;
	uint8_t *next_out;
	size_t available_out;
	size_t total_out;

	if (input_size > MAX_VALUE_LENGTH)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("input value length %zu exceeds maximum length %u", input_size, MAX_VALUE_LENGTH)));

	output_allocated = input_size + (input_size / 3);
	if (output_allocated < input_size)
	{
		/* overflow: shouldn't happen due to MAX_VALUE_LENGTH */
		elog(ERROR, "internal error: output_allocated < input_size");
	}
	else if (output_allocated < 512)
		output_allocated = 512;

	BrotliDecoderState* dec = BrotliDecoderCreateInstance(pgbrotli_alloc, pgbrotli_free, NULL);
	if (dec == NULL)
		elog(ERROR, "could not initialize decompression state");

	available_in = input_size;
	output = (bytea *) palloc(VARHDRSZ + output_allocated);
	available_out = output_allocated;
	next_out = (uint8_t *) VARDATA(output);
	size_t realloc_divisor = 4;
	for (;;)
	{
		BrotliDecoderResult res =
			BrotliDecoderDecompressStream(dec, &available_in, (const uint8_t **) &next_in,
										  &available_out, &next_out, &total_out);
		if (res == BROTLI_DECODER_RESULT_SUCCESS)
			break;

		if (res == BROTLI_DECODER_RESULT_ERROR)
		{
			char *error = pstrdup(BrotliDecoderErrorString(BrotliDecoderGetErrorCode(dec)));
			BrotliDecoderDestroyInstance(dec);
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("internal error: %s", error)));
		}
		else if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT)
		{
			BrotliDecoderDestroyInstance(dec);
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("corrupted compressed data")));
		}
		else if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT)
		{
			ptrdiff_t curroff;

			if (output_allocated >= MAX_VALUE_LENGTH)
			{
				BrotliDecoderDestroyInstance(dec);
				ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						 errmsg("decompressed value exceeds maximum length %u", MAX_VALUE_LENGTH)));
			}
			curroff = next_out - (uint8_t *) VARDATA(output);
			available_out = input_size / realloc_divisor;
			if (available_out < 4096 / realloc_divisor)
				available_out = 4096 / realloc_divisor;
			if (realloc_divisor > 2)
				realloc_divisor--;
			if (output_allocated + available_out < output_allocated)
			{
				BrotliDecoderDestroyInstance(dec);
				ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						 errmsg("decompressed value exceeds maximum length %u", MAX_VALUE_LENGTH)));
			}
			output_allocated += available_out;
			output = repalloc(output, VARHDRSZ + output_allocated);
			next_out = (uint8_t *) VARDATA(output) + curroff;
		}
		else
		{
			BrotliDecoderDestroyInstance(dec);
			elog(ERROR, "internal error: unexpected return value from decompressor");
		}
	}
	BrotliDecoderDestroyInstance(dec);
	SET_VARSIZE(output, VARHDRSZ + total_out);
	PG_RETURN_TEXT_P(output);
}
