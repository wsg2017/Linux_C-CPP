DROP TABLE IF EXISTS `index_failure_t`;
CREATE TABLE `index_failure_t` (
	`id` INT(11) NOT NULL AUTO_INCREMENT,
	`name` VARCHAR(255) DEFAULT NULL,
	`cid` INT(11) DEFAULT NULL,
	`age` SMALLINT DEFAULT 0,
	`score` SMALLINT DEFAULT 0,
	`phonenumber` VARCHAR(20),
	PRIMARY KEY (`id`),
	KEY `name_idx` (`name`),
	KEY `phone_idx` (`phonenumber`)
)ENGINE = INNODB AUTO_INCREMENT=0 DEFAULT CHARSET = utf8;

INSERT INTO `index_failure_t` (`name`, `cid`, `age`, `score`, `phonenumber`)
VALUES
	('谢某某', 10001, 12, 99, '13100000000'),
	('廖某某', 10002, 13, 98, '13700000000'),
	('吴某某', 10003, 14, 97, '17300000000'),
	('王某某', 10004, 15, 100, '13900000000');
	
	select * from `index_failure_t`;
	
explain select * from index_failure_t where name like '%谢';

explain select * from index_failure_t where length(name) = 9;

explain select * from index_failure_t where id = 3-1;

-- MySQL 遇到字符串和数字比较时，会自动将字符串转换为数字；
explain select * from index_failure_t where phonenumber = 13700000000;
-- <=> explain select * from index_failure_t where cast(phonenumber as signed int) = 13700000000;

explain select * from index_failure_t where id = 3 or age = 18;
