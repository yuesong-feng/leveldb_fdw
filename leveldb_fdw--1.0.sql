/* contrib/leveldb_fdw/leveldb_fdw--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION leveldb_fdw" to load this file. \quit

CREATE FUNCTION leveldb_fdw_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER leveldb_fdw
    HANDLER leveldb_fdw_handler;
