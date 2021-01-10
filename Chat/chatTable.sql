create table cso_id(
userid varchar(10) COLLATE KOREAN_WANSUNG_CS_AS not null,
password varchar(10) not null,
nickname varchar(20) not null,
regdate datetime not null,
lastlogdate datetime,
loginyn int not null DEFAULT 0,
adminyn int not null DEFAULT 0
);

alter table cso_id
add constraint pk_cso_id primary key(userid);

create table cso_login(
userid varchar(10) COLLATE KOREAN_WANSUNG_CS_AS not null,
logindate datetime not null,
nickname varchar(20) not null
);

alter table cso_login
add constraint pk_cso_login primary key(userid, logindate);

alter table cso_login
add constraint fk_cso_login foreign key(userid) references cso_id(userid) on delete cascade;

create partition function part_direct_orderdate ( datetime )
as range RIGHT
for values (
   '2019'
  ,'2020'
  ,'2021'
);

create partition SCHEME cso_direct_orderdate
as partition part_direct_orderdate
all to ( [primary]);

create table cso_direction(
nickname varchar(20) not null,
logdate datetime not null,
status int not null,
direction int not null,
message varchar(512) not null
) ON cso_direct_orderdate (logdate);

alter table cso_direction
add constraint pk_cso_direction primary key(logdate ,nickname);


create partition function part_chat_orderdate ( datetime )
as range RIGHT
for values (
   '2019'
  ,'2020'
  ,'2021'
);

create partition SCHEME cso_chat_orderdate
as partition part_chat_orderdate
all to ( [primary]);

create table cso_chatting(
nickname varchar(20) not null,
logdate datetime not null,
roomname varchar(10) not null,
message varchar(512) not null
) ON cso_chat_orderdate (logdate);

alter table cso_chatting
add constraint pk_cso_chatting primary key(logdate , nickname);


create table cso_relation(
relationfrom varchar(10) COLLATE KOREAN_WANSUNG_CS_AS not null,
relationto varchar(10) COLLATE KOREAN_WANSUNG_CS_AS not null,
relationcode int not null,
regdate datetime not null
);

alter table cso_relation
add constraint pk_cso_relation primary key(relationfrom , relationcode , relationto);

alter table cso_relation
add constraint fk_cso_relation foreign key(relationfrom) references cso_id(userid) on delete cascade;


create table daily (
regdate varchar(10) not null,
uniqueuser smallint,
chattraffic int
);

alter table daily
add constraint pk_daily primary key(regdate);

create table cso_filerecv (
nickname varchar(20) not null,
filename varchar(100) not null,
regdate datetime not null,
bytes bigint not null
);

alter table cso_filerecv
add constraint pk_cso_filerecv primary key(nickname, regdate);

create nonclustered index idx_filerecv_1
on cso_filerecv(regdate, nickname)

