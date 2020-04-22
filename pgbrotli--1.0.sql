-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgbrotli" to load this file. \quit

CREATE FUNCTION pgbrotli_compress(input text)
    RETURNS bytea
    AS 'pgbrotli', 'pgbrotli_compress_text'
    LANGUAGE c
    RETURNS NULL ON NULL INPUT
    IMMUTABLE
    PARALLEL SAFE
    ;

CREATE FUNCTION pgbrotli_compress(input bytea)
    RETURNS bytea
    AS 'pgbrotli', 'pgbrotli_compress_bytea'
    LANGUAGE c
    RETURNS NULL ON NULL INPUT
    IMMUTABLE
    PARALLEL SAFE
    ;

CREATE FUNCTION pgbrotli_decompress(input bytea)
    RETURNS bytea
    AS 'pgbrotli', 'pgbrotli_decompress'
    LANGUAGE c
    RETURNS NULL ON NULL INPUT
    IMMUTABLE
    PARALLEL SAFE
    ;
