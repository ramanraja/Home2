CREATE TABLE Device (
  DevNo int(11) NOT NULL AUTO_INCREMENT,
  OrgId int(11) DEFAULT 1,
  GroupId varchar(32) DEFAULT NULL,
  DeviceId varchar(32) UNIQUE NOT NULL,
  Enabled tinyint(4) DEFAULT 1,
  PRIMARY KEY (DevNo)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

select * from Device;
INSERT INTO Device (DeviceId, OrgId, GroupId) SELECT distinct DeviceId, OrgId, GroupId from IotData;  
DELETE FROM Device WHERE GroupId='G2' LIMIT 100;  -- limit is rquired to bypass safe mode
-- INSERT INTO Device (DeviceId, OrgId, GroupId) SELECT distinct DeviceId, OrgId, GroupId from IotData WHERE NOT EXISTS (SELECT DeviceId FROM IotData);

-- do this periodically to add all new devices to theis table:
TRUNCATE TABLE Device;  -- careful !
INSERT INTO Device (DeviceId, OrgId, GroupId) SELECT distinct DeviceId, OrgId, GroupId from IotData; 
select * from Device;