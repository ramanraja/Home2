# MySQL database access
# Takes values from incoming MQTT trigger and inserts them into the DB
# Trigger (new) :  SELECT *, topic() as topic FROM 'myorg/+/status/#'

import sys
#import logging
import pymysql
import json

rds_host = 'my.xxxxxxx.us-east-2.rds.amazonaws.com' 
user_name = 'user'
passwd = 'passwd'
db_name = 'mydb'
port = 3306

# Note how a long string can be split across multiple lines using brackets
# Note how the double quotes can be retained all the way upto the SQL statement
SQL = ('insert into IotEvent '
       '(OrgId, GroupId, DeviceId, Relays, EventCode, EventText) '
       'values ({org},"{gro}","{dev}","{rel}","{cod}", "{tex}")')

# place holder data object
data = {
    "ORG" : 1,   # TODO: insert the org_id from session data
    "GRO" : "G0", 
    "DEV" : "000000", 
    "REL" : "00",    # while rebooting, relay status will be 00
    "COD" : "X",
    "TEX" : "X"
}

# NOTE: better to put the connection handling at the top of the lambda file, so you can refer to it
# from multiple functions
       
conn = None       
try:
    conn = pymysql.connect(rds_host, user=user_name, passwd=passwd, db=db_name, connect_timeout=8)
    print ("--- Connected to RDS mysql.")
except Exception as e:
    print("---- Could not connect to MySql: ", e)
    #sys.exit()  # TODO: where to exit ?! if not exit, what else to do?

def lambda_handler(event, context):
    if (conn == None):
        print("---- Database connection is invalid")
        return ("DB connection failure")
    try:
        print(json.dumps(event))
        if ('B' in event):      # todo: record 'I' packets also ? Or just show them on the UI ?
            print("-- 8266 restarted")
            data['COD'] = 'B'   # event code
            data['TEX'] = event['B']
        elif ('S' in event): 
            print("-- Relay  operated")
            data['COD'] = 'S'   # event code
            data['REL'] = event['S']        
        else :
            return ("Non-event packet")
        topic_fragments = event['topic'].split('/')
        # TODO: verify if the org_id in topic matches the org_id in session
        data['DEV'] = topic_fragments[-1] # device id
        data['GRO'] = topic_fragments[-2] # group id
        with conn.cursor() as cur:
            #cur.execute('use intof_iot')
            cur.execute(get_sql())
            conn.commit()
            cur.execute("select count(*) from IotEvent")
            print ("-- Number of rows in table: ", end='')
            for row in cur:
                print(row)
        return ("DB operation succeeded")
    except Exception as e:
        print ('---- Exception ! ', e)
        return ('DB opertion failed')
        
def get_sql():
    # .format does not destroy the original string
    sqlstr = SQL.format (org=data['ORG'], gro=data['GRO'], dev=data['DEV'], 
                         rel=data['REL'], cod=data['COD'], tex=data['TEX'])
    print (sqlstr)
    return(sqlstr)
    
'''       
# unit test
if (__name__ == '__main__'):       
    event = {"B":"boooot !", "topic":"myorg/myapp/status/mygroup/mydeviceid"}
    print (lambda_handler(event, 'context'))
'''


