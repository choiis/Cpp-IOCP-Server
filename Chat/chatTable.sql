CREATE table cso_id(
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

CREATE table cso_login(
userid varchar(10) COLLATE KOREAN_WANSUNG_CS_AS not null,
logindate datetime not null,
nickname varchar(20) not null
);

alter table cso_login
add constraint pk_cso_login primary key(userid, logindate);

alter table cso_login
add constraint fk_cso_login foreign key(userid) references cso_id(userid) on delete cascade;

CREATE table cso_direction(
nickname varchar(20) not null,
logdate datetime not null,
status int not null,
direction int not null,
message varchar(512) not null
);

alter table cso_direction
add constraint pk_cso_direction primary key(logdate ,nickname);


CREATE table cso_chatting(
nickname varchar(20) not null,
logdate datetime not null,
roomname varchar(10) not null,
message varchar(512) not null
);

alter table cso_chatting
add constraint pk_cso_chatting primary key(logdate , nickname);

CREATE table cso_relation(
relationfrom varchar(10) COLLATE KOREAN_WANSUNG_CS_AS not null,
relationto varchar(10) COLLATE KOREAN_WANSUNG_CS_AS not null,
relationcode int not null,
regdate datetime not null
);

alter table cso_relation
add constraint pk_cso_relation primary key(relationfrom , relationcode , relationto);

alter table cso_relation
add constraint fk_cso_relation foreign key(relationfrom) references cso_id(userid) on delete cascade;
