
CREATE TABLE IotData (
  SlNo int(11) NOT NULL AUTO_INCREMENT,
  OrgId int(11) DEFAULT 1,
  GroupId varchar(32) DEFAULT NULL,
  DeviceId varchar(32) DEFAULT NULL,
  Alive tinyint(4) DEFAULT 1,
  Enabled tinyint(4) DEFAULT 1,
  Relays varchar(8) DEFAULT "00",
  Light smallint(4) DEFAULT 0,
  Temperature float DEFAULT 0.0,
  Humidity float DEFAULT 0.0,
  HIndex float DEFAULT 0.0,
  Pir smallint(4) DEFAULT 0,
  Radar smallint(4) DEFAULT 0,
  Timestamp datetime DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (SlNo),
  KEY FK_ORG_ID_idx (OrgId),
  CONSTRAINT FK_ORG_ID FOREIGN KEY (OrgId) REFERENCES Organisation (OrgId) ON DELETE NO ACTION ON UPDATE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;


ALTER TABLE IotData  DROP COLUMN Alive, DROP COLUMN Enabled;
  
CREATE TABLE IotData (
  SlNo int(11) NOT NULL AUTO_INCREMENT,
  OrgId int(11) DEFAULT 1,
  GroupId varchar(32) DEFAULT NULL,
  DeviceId varchar(32) DEFAULT NULL,
  Relays varchar(8) DEFAULT "00",
  Light smallint(4) DEFAULT 0,
  Temperature float DEFAULT 0.0,
  Humidity float DEFAULT 0.0,
  HIndex float DEFAULT 0.0,
  Pir smallint(4) DEFAULT 0,
  Radar smallint(4) DEFAULT 0,
  Timestamp datetime DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (SlNo),
  KEY FK_ORG_ID_idx (OrgId),
  CONSTRAINT FK_ORG_ID FOREIGN KEY (OrgId) REFERENCES Organisation (OrgId) ON DELETE NO ACTION ON UPDATE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;


  
CREATE TABLE Organisation (
  OrgId int(11) NOT NULL AUTO_INCREMENT,
  OrgName varchar(100) DEFAULT NULL,
  PRIMARY KEY (OrgId)
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=latin1