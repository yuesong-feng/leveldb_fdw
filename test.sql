CREATE EXTENSION leveldb_fdw;
CREATE SERVER leveldb_server FOREIGN DATA WRAPPER leveldb_fdw;
CREATE FOREIGN TABLE leveldb ( val int ) SERVER leveldb_server;
SELECT * FROM leveldb;
