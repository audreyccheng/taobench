drop table if exists objects;
create table objects(
	id bigint primary key,
	timestamp bigint,
	value varchar(150));
drop table if exists edges;
create table edges(
	id1 bigint,
	id2 bigint,
	type varchar(63),
	timestamp bigint,
	value varchar(150),
	primary key clustered (id1, id2, type));
