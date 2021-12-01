create user if not exists 'benchmark'@'%' identified by 'taobenchmark123';
grant all privileges on *.* to 'benchmark'@'%';
drop table if exists objects;
create table objects(
	id varchar(63) primary key clustered,
	timestamp bigint,
	value varchar(150));
drop table if exists edges;
create table edges(
	id1 varchar(63),
	id2 varchar(63),
	type varchar(63),
	timestamp bigint,
	value varchar(150),
	primary key clustered (id1, id2, type));
