show create table IotData;
show create table Organisation;
select count(*) from IotData;
select count(*) from IotData where GroupId='G1';
select count(*) from IotData where GroupId='G2';
select * from IotData; -- brings 1000 records by default
select * from IotData limit 100;
select * from IotData order by SlNo desc limit 20; -- see latest 20 records
select * from IotData where GroupId='G1' order by SlNo desc limit 20; -- see latest 20 records from G2
select * from IotData where GroupId='G2' order by SlNo desc limit 20; -- see latest 20 records from G2
select distinct DeviceId, GroupId from IotData;

SELECT NOW();
SELECT CONVERT_TZ(NOW(),'SYSTEM','Asia/Calcutta');
